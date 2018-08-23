/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_ordering_service_impl.hpp"

#include <unordered_set>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include "backend/protobuf/proposal.hpp"
#include "backend/protobuf/transaction.hpp"
#include "datetime/time.hpp"
#include "interfaces/iroha_internal/proposal.hpp"
#include "interfaces/transaction.hpp"

using namespace iroha::ordering;

/**
 * First round after successful committing block
 */
const iroha::ordering::transport::RejectRoundType kFirstRound = 1;

OnDemandOrderingServiceImpl::OnDemandOrderingServiceImpl(
    size_t transaction_limit,
    size_t number_of_proposals,
    const transport::RoundType &initial_round)
    : transaction_limit_(transaction_limit),
      number_of_proposals_(number_of_proposals),
      log_(logger::log("OnDemandOrderingServiceImpl")) {
  onCollaborationOutcome(initial_round);
}

// -------------------------| OnDemandOrderingService |-------------------------

void OnDemandOrderingServiceImpl::onCollaborationOutcome(
    transport::RoundType round) {
  log_->info(
      "onCollaborationOutcome => round[{}, {}]", round.first, round.second);
  // exclusive write lock
  std::lock_guard<std::shared_timed_mutex> guard(lock_);
  log_->info("onCollaborationOutcome => write lock is acquired");

  packNextProposals(round);
  tryErase();
}

// ----------------------------| OdOsNotification |-----------------------------

void OnDemandOrderingServiceImpl::onTransactions(transport::RoundType round,
                                                 CollectionType transactions) {
  // read lock
  std::shared_lock<std::shared_timed_mutex> guard(lock_);
  log_->info("onTransactions => collections size = {}, round[{}, {}]",
             transactions.size(),
             round.first,
             round.second);

  auto it = current_proposals_.find(round);
  if (it != current_proposals_.end()) {
    std::for_each(transactions.begin(), transactions.end(), [&it](auto &obj) {
      it->second.push(std::move(obj));
    });
    log_->info("onTransactions => collection is inserted");
  }
}

boost::optional<OnDemandOrderingServiceImpl::ProposalType>
OnDemandOrderingServiceImpl::onRequestProposal(transport::RoundType round) {
  // read lock
  std::shared_lock<std::shared_timed_mutex> guard(lock_);
  auto proposal = proposal_map_.find(round);
  if (proposal != proposal_map_.end()) {
    return clone(*proposal->second);
  } else {
    return boost::none;
  }
}

// ---------------------------------| Private |---------------------------------

void OnDemandOrderingServiceImpl::packNextProposals(
    transport::RoundType round) {
  auto close_round = [this](transport::RoundType round) {
    auto it = current_proposals_.find(round);
    if (it != current_proposals_.end()) {
      if (not it->second.empty()) {
        proposal_map_.emplace(round, emitProposal(round));
        log_->info("packNextProposal: data has been fetched for round[{}, {}]",
                   round.first,
                   round.second);
        round_queue_.push(round);
      }
      current_proposals_.erase(it);
    }
  };

  close_round({round.first, round.second + 1});
  if (round.second == kFirstRound) {
    // new block round
    close_round({round.first + 1, round.second});

    current_proposals_.clear();
    for (size_t i = 0; i <= 2; ++i) {
      current_proposals_[{round.first + i, round.second + 2 - i}];
    }
  } else {
    // new reject round
    current_proposals_[{round.first, round.second + 2}];
  }
}

OnDemandOrderingServiceImpl::ProposalType
OnDemandOrderingServiceImpl::emitProposal(transport::RoundType round) {
  iroha::protocol::Proposal proto_proposal;
  proto_proposal.set_height(round.first);
  proto_proposal.set_created_time(iroha::time::now());
  log_->info(
      "Mutable proposal generation, round[{}, {}]", round.first, round.second);

  TransactionType current_tx;
  using ProtoTxType = shared_model::proto::Transaction;
  std::vector<TransactionType> collection;
  std::unordered_set<std::string> inserted;

  // outer method should guarantee availability of at least one transaction in
  // queue, also, code shouldn't fetch all transactions from queue. The rest
  // will be lost.
  auto &current_proposal = current_proposals_[round];
  while (current_proposal.try_pop(current_tx)
         and collection.size() < transaction_limit_
         and inserted.insert(current_tx->hash().hex()).second) {
    collection.push_back(std::move(current_tx));
  }
  log_->info("Number of transactions in proposal = {}", collection.size());
  auto proto_txes = collection | boost::adaptors::transformed([](auto &tx) {
                      return static_cast<const ProtoTxType &>(*tx);
                    });
  boost::for_each(proto_txes, [&proto_proposal](auto &&proto_tx) {
    *(proto_proposal.add_transactions()) = std::move(proto_tx.getTransport());
  });

  return std::make_unique<shared_model::proto::Proposal>(
      std::move(proto_proposal));
}

void OnDemandOrderingServiceImpl::tryErase() {
  if (round_queue_.size() >= number_of_proposals_) {
    auto &round = round_queue_.front();
    proposal_map_.erase(round);
    log_->info("tryErase: erased round[{}, {}]", round.first, round.second);
    round_queue_.pop();
  }
}
