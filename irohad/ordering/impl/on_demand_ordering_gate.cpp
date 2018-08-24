/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_ordering_gate.hpp"

#include "interfaces/iroha_internal/proposal.hpp"
#include "interfaces/iroha_internal/transaction_batch.hpp"

using namespace iroha;
using namespace iroha::ordering;

namespace {
  /**
   * Shortcut for calculating next reject round
   * @param round current round
   * @return next reject round
   */
  constexpr transport::RoundType nextRejectRound(
      const transport::RoundType &round) {
    return {round.first, round.second + 1};
  }
}  // namespace

OnDemandOrderingGate::OnDemandOrderingGate(
    std::shared_ptr<OnDemandOrderingService> ordering_service,
    std::shared_ptr<transport::OdOsNotification> network_client,
    std::shared_ptr<network::ProposalGate> proposal_gate,
    std::function<shared_model::interface::TransactionBatch(
        std::shared_ptr<const shared_model::interface::Transaction>)>
        batch_factory,
    rxcpp::observable<BlockRoundEventType> events,
    transport::RoundType initial_round)
    : ordering_service_(std::move(ordering_service)),
      network_client_(std::move(network_client)),
      proposal_gate_(std::move(proposal_gate)),
      batch_factory_(std::move(batch_factory)),
      subscription_(events.subscribe([this](auto event) {
        // exclusive lock
        std::lock_guard<std::shared_timed_mutex> lock(mutex_);

        visit_in_place(event,
                       [this](BlockEvent block) {
                         // block committed, increment block round
                         current_round_ = {block.height, 1};
                       },
                       [this](EmptyEvent empty) {
                         current_round_ = nextRejectRound(current_round_);
                       });
        this->vote();
      })),
      current_round_(initial_round) {}

void OnDemandOrderingGate::propagateTransaction(
    std::shared_ptr<const shared_model::interface::Transaction> transaction)
    const {
  propagateBatch(batch_factory_(std::move(transaction)));
}

void OnDemandOrderingGate::propagateBatch(
    const shared_model::interface::TransactionBatch &batch) const {
  // shared lock
  std::shared_lock<std::shared_timed_mutex> lock(mutex_);

  network_client_->onTransactions(current_round_, batch.transactions());
}

rxcpp::observable<std::shared_ptr<shared_model::interface::Proposal>>
OnDemandOrderingGate::on_proposal() {
  using RetType = decltype(on_proposal());
  return proposal_gate_->outcomes().flat_map([this](auto outcome) {
    // exclusive lock
    std::lock_guard<std::shared_timed_mutex> lock(mutex_);

    // reject case actions
    auto reject_case = [this](const auto &round) -> RetType {
      current_round_ = nextRejectRound(round);
      this->vote();
      return rxcpp::observable<>::empty<RetType::value_type>();
    };

    return visit_in_place(
        outcome,
        [&](network::ProposalCommit commit) {
          // return the proposal if it is not empty
          auto result = commit.proposal | [](auto &&proposal) {
            return boost::optional<RetType>(
                rxcpp::observable<>::just(std::move(proposal)));
          };

          // call reject case actions otherwise
          return result.value_or_eval(
              [&] { return reject_case(commit.round); });
        },
        [&](network::ProposalReject reject) {
          return reject_case(reject.round);
        });
  });
}

void OnDemandOrderingGate::setPcs(
    const iroha::network::PeerCommunicationService &pcs) {}

void OnDemandOrderingGate::vote() {
  // notify our ordering service about new round
  ordering_service_->onCollaborationOutcome(current_round_);

  // request proposal for the current round
  auto proposal = network_client_->onRequestProposal(current_round_);

  // vote for the object received from the network
  proposal_gate_->vote({std::move(proposal), current_round_});
}
