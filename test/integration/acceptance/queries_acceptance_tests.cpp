/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include "backend/protobuf/transaction.hpp"
#include "framework/integration_framework/integration_test_framework.hpp"
#include "framework/specified_visitor.hpp"
#include "integration/acceptance/acceptance_fixture.hpp"
#include "interfaces/permissions.hpp"

using namespace integration_framework;
using namespace shared_model;

class QueriesAcceptanceTests : public AcceptanceFixture {
 public:
  QueriesAcceptanceTests()
      : invalidPrivateKey(kUserKeypair.privateKey().hex()),
        invalidPublicKey(kUserKeypair.publicKey().hex()) {
    invalidPrivateKey[0] == '9' || invalidPrivateKey[0] == 'f'
        ? invalidPrivateKey[0] = --invalidPrivateKey[0]
        : invalidPrivateKey[0] = ++invalidPrivateKey[0];
    invalidPublicKey[0] == '9' || invalidPublicKey[0] == 'f'
        ? invalidPublicKey[0] = --invalidPublicKey[0]
        : invalidPublicKey[0] = ++invalidPublicKey[0];
  };
  std::string invalidPrivateKey;
  std::string invalidPublicKey;

  const std::string NonExistentUserId = "aaaa@aaaa";
};

/**
 * @given query with a non-existent creator_account_id
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateful validation
 */
TEST_F(QueriesAcceptanceTests, NonExistentCreatorId) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatefulFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now())
                   .creatorAccountId(NonExistentUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(boost::size(block->transactions()), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with an 1 hour old UNIX time
 * @when execute any correct query with needed permitions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTests, OneHourOldTime) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW(boost::apply_visitor(
        framework::SpecifiedVisitor<shared_model::interface::RolesResponse>(),
        queryResponse.get()));
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with more than 24 hour old UNIX time
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, More24HourOldTime) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)
                                                 - std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with an 24 hour old UNIX time
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, TwentyFourHourOldTime) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with less than 24 hour old UNIX time
 * @when execute any correct query with needed permitions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTests, Less24HourOldTime) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW(boost::apply_visitor(
        framework::SpecifiedVisitor<shared_model::interface::RolesResponse>(),
        queryResponse.get()));
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)
                                                 + std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with less than 5 minutes from future UNIX time
 * @when execute any correct query with needed permitions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTests, LessFiveMinutesFromFuture) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW(boost::apply_visitor(
        framework::SpecifiedVisitor<shared_model::interface::RolesResponse>(),
        queryResponse.get()));
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)
                                                 - std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with 5 minutes from future UNIX time
 * @when execute any correct query with needed permitions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTests, FiveMinutesFromFuture) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW(boost::apply_visitor(
        framework::SpecifiedVisitor<shared_model::interface::RolesResponse>(),
        queryResponse.get()));
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with more than 5 minutes from future UNIX time
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, MoreFiveMinutesFromFuture) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)
                                                 + std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with 10 minutes from future UNIX time
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, TenMinutesFromFuture) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(10)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain invalid sign but valid public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, InvalidSignValidPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  crypto::Keypair kInvalidSignValidPubKeypair = crypto::Keypair(
      kUserKeypair.publicKey(),
      crypto::PrivateKey(crypto::Blob::fromHexString(invalidPrivateKey)));

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now())
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kInvalidSignValidPubKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain valid sign but invalid public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, ValidSignInvalidPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  crypto::Keypair kValidSignInvalidPubKeypair = crypto::Keypair(
      crypto::PublicKey(crypto::Blob::fromHexString(invalidPublicKey)),
      kUserKeypair.privateKey());

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now())
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kValidSignInvalidPubKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain invalid sign and invalid public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, FullyInvalidKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  crypto::Keypair kFullyInvalidKeypair = crypto::Keypair(
      crypto::PublicKey(crypto::Blob::fromHexString(invalidPublicKey)),
      crypto::PrivateKey(crypto::Blob::fromHexString(invalidPrivateKey)));

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now())
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kFullyInvalidKeypair)
                   .finish();

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain empty sign and valid public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, EmptySignValidPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto proto_query = TestUnsignedQueryBuilder()
                         .createdTime(iroha::time::now())
                         .creatorAccountId(kUserId)
                         .queryCounter(1)
                         .getRoles()
                         .build()
                         .signAndAddSignature(kUserKeypair)
                         .finish()
                         .getTransport();

  proto_query.clear_signature();
  auto query = shared_model::proto::Query(proto_query);

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain valid sign and empty public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, ValidSignEmptyPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto proto_query = TestUnsignedQueryBuilder()
                         .createdTime(iroha::time::now())
                         .creatorAccountId(kUserId)
                         .queryCounter(1)
                         .getRoles()
                         .build()
                         .signAndAddSignature(kUserKeypair)
                         .finish()
                         .getTransport();

  proto_query.mutable_signature()->clear_pubkey();
  auto query = shared_model::proto::Query(proto_query);

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain empty sign and empty public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, FullyEmptyPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  auto proto_query = TestUnsignedQueryBuilder()
                         .createdTime(iroha::time::now())
                         .creatorAccountId(kUserId)
                         .queryCounter(1)
                         .getRoles()
                         .build()
                         .signAndAddSignature(kUserKeypair)
                         .finish()
                         .getTransport();

  proto_query.clear_signature();
  proto_query.mutable_signature()->clear_pubkey();
  auto query = shared_model::proto::Query(proto_query);

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain invalid sign and empty public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, InvalidSignEmptyPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  shared_model::crypto::Keypair kInvalidSignEmptyPubKeypair = crypto::Keypair(
      kUserKeypair.publicKey(),
      crypto::PrivateKey(crypto::Blob::fromHexString(invalidPrivateKey)));

  auto proto_query = TestUnsignedQueryBuilder()
                         .createdTime(iroha::time::now())
                         .creatorAccountId(kUserId)
                         .queryCounter(1)
                         .getRoles()
                         .build()
                         .signAndAddSignature(kInvalidSignEmptyPubKeypair)
                         .finish()
                         .getTransport();

  proto_query.mutable_signature()->clear_pubkey();
  auto query = shared_model::proto::Query(proto_query);

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}

/**
 * @given query with Keypair which contain empty sign and invalid public key
 * @when execute any correct query with needed permitions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTests, EmptySignInvalidPubKeypair) {
  auto checkQuery = [](auto &queryResponse) {
    ASSERT_NO_THROW({
      boost::apply_visitor(
          framework::SpecifiedVisitor<
              shared_model::interface::StatelessFailedErrorResponse>(),
          boost::apply_visitor(
              framework::SpecifiedVisitor<
                  shared_model::interface::ErrorQueryResponse>(),
              queryResponse.get())
              .get());
    });
  };

  crypto::Keypair kEmptySignInvalidPubKeypair = crypto::Keypair(
      crypto::PublicKey(crypto::Blob::fromHexString(invalidPublicKey)),
      kUserKeypair.privateKey());

  auto proto_query = TestUnsignedQueryBuilder()
                         .createdTime(iroha::time::now())
                         .creatorAccountId(kUserId)
                         .queryCounter(1)
                         .getRoles()
                         .build()
                         .signAndAddSignature(kEmptySignInvalidPubKeypair)
                         .finish()
                         .getTransport();

  proto_query.clear_signature();
  auto query = shared_model::proto::Query(proto_query);

  IntegrationTestFramework(1)
      .setInitialState(kAdminKeypair)
      .sendTx(makeUserWithPerms(
          {shared_model::interface::permissions::Role::kGetRoles}))
      .skipProposal()
      .checkBlock(
          [](auto &block) { ASSERT_EQ(block->transactions().size(), 1); })
      .sendQuery(query, checkQuery);
}
