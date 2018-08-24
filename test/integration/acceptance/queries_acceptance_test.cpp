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
#include "utils/query_error_response_visitor.hpp"

using namespace integration_framework;
using namespace shared_model;

class QueriesAcceptanceTest : public AcceptanceFixture {
 public:
  QueriesAcceptanceTest()
      : itf(1),
        invalidPrivateKey(kUserKeypair.privateKey().hex()),
        invalidPublicKey(kUserKeypair.publicKey().hex()) {
    /*
     * It's deliberately get broken the public key and privite key to simulate a
     * non-valid signature and public key and use their combinations in the
     * tests below
     * Both keys are hex values represented as a std::string so
     * characters can be values only in the range 0-9 and a-f
     */
    invalidPrivateKey[0] == '9' or invalidPrivateKey[0] == 'f'
        ? --invalidPrivateKey[0]
        : ++invalidPrivateKey[0];
    invalidPublicKey[0] == '9' or invalidPublicKey[0] == 'f'
        ? --invalidPublicKey[0]
        : ++invalidPublicKey[0];
  };
  void SetUp() {
    itf.setInitialState(kAdminKeypair)
        .sendTx(makeUserWithPerms({interface::permissions::Role::kGetRoles}))
        .skipProposal()
        .checkBlock([](auto &block) {
          ASSERT_EQ(boost::size(block->transactions()), 1);
        });
  };

  template<typename F>
  auto checkIfStatefulFailed(F& queryResponse)
  {
    ASSERT_TRUE(
        boost::apply_visitor(interface::QueryErrorResponseChecker<
                                 interface::StatefulFailedErrorResponse>(),
                             queryResponse.get()))
                  << "Actual response: " << queryResponse.toString();
  };
  —
  template<typename F>
  auto checkIfStatelessFailed(F& queryResponse)
  {
    ASSERT_TRUE(
        boost::apply_visitor(interface::QueryErrorResponseChecker<
                                 interface::StatelessFailedErrorResponse>(),
                             queryResponse.get()))
                  << "Actual response: " << queryResponse.toString();
  };


  template<typename F>
  auto checkIfSuccess(F& queryResponse)
  {
    ASSERT_NO_THROW(boost::apply_visitor(
        framework::SpecifiedVisitor<interface::RolesResponse>(),
        queryResponse.get()));

  }

  IntegrationTestFramework itf;
  std::string invalidPrivateKey;
  std::string invalidPublicKey;
  const std::string NonExistentUserId = "aaaa@aaaa";
};

/**
 * @given query with a non-existent creator_account_id
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateful validation
 */
TEST_F(QueriesAcceptanceTest, NonExistentCreatorId) {

  auto check = [this](auto &queryResponse){checkIfStatefulFailed(queryResponse);};

  auto query = baseQry()
                   .creatorAccountId(NonExistentUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with an 1 hour old UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTest, OneHourOldTime) {

  auto check = [this](auto &queryResponse){checkIfSuccess(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with more than 24 hour old UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, More24HourOldTime) {
  
  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)
                                                 - std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with an 24 hour old UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, TwentyFourHourOldTime) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with less than 24 hour old UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTest, Less24HourOldTime) {

  auto check = [this](auto &queryResponse){checkIfSuccess(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::hours(-24)
                                                 + std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with less than 5 minutes from future UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTest, LessFiveMinutesFromFuture) {

  auto check = [this](auto &queryResponse){checkIfSuccess(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)
                                                 - std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with 5 minutes from future UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query returns list of roles
 */
TEST_F(QueriesAcceptanceTest, FiveMinutesFromFuture) {

  auto check = [this](auto &queryResponse){checkIfSuccess(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with more than 5 minutes from future UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, MoreFiveMinutesFromFuture) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(5)
                                                 + std::chrono::seconds(1)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with 10 minutes from future UNIX time
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, TenMinutesFromFuture) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

  auto query = TestUnsignedQueryBuilder()
                   .createdTime(iroha::time::now(std::chrono::minutes(10)))
                   .creatorAccountId(kUserId)
                   .queryCounter(1)
                   .getRoles()
                   .build()
                   .signAndAddSignature(kUserKeypair)
                   .finish();

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains invalid signature but valid public
 * key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, InvalidSignValidPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains valid signature but invalid public
 * key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, ValidSignInvalidPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains invalid signature and invalid public
 * key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, FullyInvalidKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains empty signature and valid public key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, EmptySignValidPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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
  auto query = proto::Query(proto_query);

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains valid signature and empty public key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, ValidSignEmptyPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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
  auto query = proto::Query(proto_query);

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains empty signature and empty public key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, FullyEmptyPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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
  auto query = proto::Query(proto_query);

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains invalid signature and empty public
 * key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, InvalidSignEmptyPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

  crypto::Keypair kInvalidSignEmptyPubKeypair = crypto::Keypair(
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
  auto query = proto::Query(proto_query);

  itf.sendQuery(query, check);
}

/**
 * @given query with Keypair which contains empty signature and invalid public
 * key
 * @when execute any correct query with kGetRoles permissions
 * @then the query should not pass stateless validation
 */
TEST_F(QueriesAcceptanceTest, EmptySignInvalidPubKeypair) {

  auto check = [this](auto &queryResponse){checkIfStatelessFailed(queryResponse);};

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
  auto query = proto::Query(proto_query);

  itf.sendQuery(query, check);
}
