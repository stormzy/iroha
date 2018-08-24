/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_ordering_gate.hpp"

#include <gtest/gtest.h>
#include "framework/test_subscriber.hpp"
#include "module/irohad/network/network_mocks.hpp"
#include "module/irohad/ordering/ordering_mocks.hpp"
#include "module/shared_model/interface_mocks.hpp"

using namespace iroha::ordering;
using namespace iroha::ordering::transport;
using namespace iroha::network;
using namespace framework::test_subscriber;

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::Truly;

struct OnDemandOrderingGateTest : public ::testing::Test {
  void SetUp() override {
    ordering_service = std::make_shared<MockOnDemandOrderingService>();
    notification = std::make_shared<MockOdOsNotification>();
    proposal_gate = std::make_shared<MockProposalGate>();
    ordering_gate = std::make_shared<OnDemandOrderingGate>(
        ordering_service,
        notification,
        proposal_gate,
        [this](auto tx) {
          return shared_model::interface::TransactionBatch(
              {std::const_pointer_cast<
                  shared_model::interface::types::SharedTxsCollectionType::
                      value_type::element_type>(tx)});
        },
        rounds.get_observable(),
        initial_round);
  }

  rxcpp::subjects::subject<OnDemandOrderingGate::BlockRoundEventType> rounds;
  rxcpp::subjects::subject<ProposalOutcomeType> outcomes;
  std::shared_ptr<MockOnDemandOrderingService> ordering_service;
  std::shared_ptr<MockOdOsNotification> notification;
  std::shared_ptr<MockProposalGate> proposal_gate;
  std::shared_ptr<OnDemandOrderingGate> ordering_gate;

  const RoundType initial_round = {2, 1};
};

/**
 * @given initialized ordering gate
 * @when a transaction is received
 * @then it is passed to the ordering service
 */
TEST_F(OnDemandOrderingGateTest, propagateTransaction) {
  auto tx = std::make_shared<MockTransaction>();
  OdOsNotification::CollectionType collection{tx};

  EXPECT_CALL(*notification, onTransactions(initial_round, collection))
      .Times(1);

  ordering_gate->propagateTransaction(tx);
}

/**
 * @given initialized ordering gate
 * @when a batch is received
 * @then it is passed to the ordering service
 */
TEST_F(OnDemandOrderingGateTest, propagateBatch) {
  OdOsNotification::CollectionType collection;
  shared_model::interface::TransactionBatch batch(collection);

  EXPECT_CALL(*notification, onTransactions(initial_round, collection))
      .Times(1);

  ordering_gate->propagateBatch(batch);
}

/**
 * @given initialized ordering gate
 * @when a block round event with height is received from the PCS
 * @then new proposal round based on the received height is initiated
 */
TEST_F(OnDemandOrderingGateTest, BlockEvent) {
  OnDemandOrderingGate::BlockEvent event{3};
  RoundType round{event.height, 1};

  boost::optional<OdOsNotification::ProposalType> oproposal;

  EXPECT_CALL(*ordering_service, onCollaborationOutcome(round)).Times(1);
  EXPECT_CALL(*notification, onRequestProposal(round))
      .WillOnce(Return(ByMove(std::move(oproposal))));
  EXPECT_CALL(*proposal_gate, doVote(Truly([&](ProposalVote &vote) {
    return not vote.proposal and vote.round == round;
  })))
      .Times(1);

  rounds.get_subscriber().on_next(event);
}

/**
 * @given initialized ordering gate
 * @when an empty block round event is received from the PCS
 * @then proposal reject round is initiated
 */
TEST_F(OnDemandOrderingGateTest, EmptyEvent) {
  RoundType round{initial_round.first, initial_round.second + 1};

  boost::optional<OdOsNotification::ProposalType> oproposal;

  EXPECT_CALL(*ordering_service, onCollaborationOutcome(round)).Times(1);
  EXPECT_CALL(*notification, onRequestProposal(round))
      .WillOnce(Return(ByMove(std::move(oproposal))));
  EXPECT_CALL(*proposal_gate, doVote(Truly([&](ProposalVote &vote) {
    return not vote.proposal and vote.round == round;
  })))
      .Times(1);

  rounds.get_subscriber().on_next(OnDemandOrderingGate::EmptyEvent{});
}

/**
 * @given initialized ordering gate
 * @when a commit is received from consensus
 * @then no proposal round actions are done
 * AND proposal is emitted
 */
TEST_F(OnDemandOrderingGateTest, ProposalCommit) {
  ProposalCommit commit{std::shared_ptr<shared_model::interface::Proposal>{},
                        initial_round};

  EXPECT_CALL(*proposal_gate, outcomes())
      .WillOnce(Return(outcomes.get_observable()));
  EXPECT_CALL(*ordering_service, onCollaborationOutcome(_)).Times(0);
  EXPECT_CALL(*notification, onRequestProposal(_)).Times(0);
  EXPECT_CALL(*proposal_gate, doVote(_)).Times(0);

  auto gate_wrapper =
      make_test_subscriber<CallExact>(ordering_gate->on_proposal(), 1);
  gate_wrapper.subscribe(
      [&](auto proposal) { ASSERT_EQ(proposal, commit.proposal); });

  outcomes.get_subscriber().on_next(commit);
  ASSERT_TRUE(gate_wrapper.validate());
}

/**
 * @given initialized ordering gate
 * @when an empty commit is received from consensus
 * @then proposal reject round is initiated
 * AND no proposal is emitted
 */
TEST_F(OnDemandOrderingGateTest, ProposalCommitEmpty) {
  ProposalCommit commit{boost::none, initial_round};
  RoundType round{initial_round.first, initial_round.second + 1};

  boost::optional<OdOsNotification::ProposalType> oproposal;

  EXPECT_CALL(*proposal_gate, outcomes())
      .WillOnce(Return(outcomes.get_observable()));
  EXPECT_CALL(*ordering_service, onCollaborationOutcome(round)).Times(1);
  EXPECT_CALL(*notification, onRequestProposal(round))
      .WillOnce(Return(ByMove(std::move(oproposal))));
  EXPECT_CALL(*proposal_gate, doVote(Truly([&](ProposalVote &vote) {
    return not vote.proposal and vote.round == round;
  })))
      .Times(1);

  auto gate_wrapper =
      make_test_subscriber<CallExact>(ordering_gate->on_proposal(), 0);
  gate_wrapper.subscribe();

  outcomes.get_subscriber().on_next(commit);
  ASSERT_TRUE(gate_wrapper.validate());
}

/**
 * @given initialized ordering gate
 * @when a reject is received from consensus
 * @then proposal reject round is initiated
 * AND no proposal is emitted
 */
TEST_F(OnDemandOrderingGateTest, ProposalReject) {
  ProposalReject reject{initial_round};
  RoundType round{initial_round.first, initial_round.second + 1};

  boost::optional<OdOsNotification::ProposalType> oproposal;

  EXPECT_CALL(*proposal_gate, outcomes())
      .WillOnce(Return(outcomes.get_observable()));
  EXPECT_CALL(*ordering_service, onCollaborationOutcome(round)).Times(1);
  EXPECT_CALL(*notification, onRequestProposal(round))
      .WillOnce(Return(ByMove(std::move(oproposal))));
  EXPECT_CALL(*proposal_gate, doVote(Truly([&](ProposalVote &vote) {
    return not vote.proposal and vote.round == round;
  })))
      .Times(1);

  auto gate_wrapper =
      make_test_subscriber<CallExact>(ordering_gate->on_proposal(), 0);
  gate_wrapper.subscribe();

  outcomes.get_subscriber().on_next(reject);
  ASSERT_TRUE(gate_wrapper.validate());
}
