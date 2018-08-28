/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_SHARED_MODEL_FACTORY_MOCKS_HPP
#define IROHA_SHARED_MODEL_FACTORY_MOCKS_HPP

#include <gmock/gmock.h>
#include "interfaces/common_objects/common_objects_factory.hpp"

struct MockCommonObjectsFactory
    : public shared_model::interface::CommonObjectsFactory {
  MOCK_METHOD2(createPeer,
               FactoryResult<std::unique_ptr<Peer>>(
                   const types::AddressType &address,
                   const types::PubkeyType &public_key));

  MOCK_METHOD4(createAccount,
               FactoryResult<std::unique_ptr<Account>>(
                   const types::AccountIdType &account_id,
                   const types::DomainIdType &domain_id,
                   types::QuorumType quorum,
                   const types::JsonType &jsonData));

  MOCK_METHOD3(createAccountAsset,
               FactoryResult<std::unique_ptr<AccountAsset>>(
                   const types::AccountIdType &account_id,
                   const types::AssetIdType &asset_id,
                   const Amount &balance));

  MOCK_METHOD3(createAsset,
               FactoryResult<std::unique_ptr<Asset>>(
                   const types::AssetIdType &asset_id,
                   const types::DomainIdType &domain_id,
                   types::PrecisionType precision));

  MOCK_METHOD2(createDomain,
               FactoryResult<std::unique_ptr<Domain>>(
                   const types::DomainIdType &domain_id,
                   const types::RoleIdType &default_role));

  MOCK_METHOD2(createSignature,
               FactoryResult<std::unique_ptr<Signature>>(
                   const interface::types::PubkeyType &key,
                   const interface::Signature::SignedType &signed_data));
};

#endif  // IROHA_SHARED_MODEL_FACTORY_MOCKS_HPP
