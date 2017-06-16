#include "envoy/registry/registry.h"

#include "server/configuration_impl.h"
#include "server/listener_manager_impl.h"

#include "test/mocks/server/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Server {

class ListenerManagerImplTest : public testing::Test {
public:
  ListenerManagerImplTest() {
    EXPECT_CALL(worker_factory_, createWorker_()).WillOnce(Return(worker_));
    manager_.reset(new ListenerManagerImpl(server_, listener_factory_, worker_factory_));
  }

  NiceMock<MockInstance> server_;
  MockListenerComponentFactory listener_factory_;
  MockWorker* worker_ = new MockWorker();
  NiceMock<MockWorkerFactory> worker_factory_;
  std::unique_ptr<ListenerManagerImpl> manager_;
  NiceMock<MockGuardDog> guard_dog_;
};

class ListenerManagerImplWithRealFiltersTest : public ListenerManagerImplTest {
public:
  ListenerManagerImplWithRealFiltersTest() {
    // Use real filter loading by default.
    ON_CALL(listener_factory_, createFilterFactoryList(_, _))
        .WillByDefault(
            Invoke([&](const std::vector<Json::ObjectSharedPtr>& filters,
                       Server::Configuration::FactoryContext& context)
                       -> std::vector<Server::Configuration::NetworkFilterFactoryCb> {
                         return Server::ProdListenerComponentFactory::createFilterFactoryList_(
                             filters, server_, context);
                       }));
  }
};

TEST_F(ListenerManagerImplWithRealFiltersTest, EmptyFilter) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": []
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  manager_->addOrUpdateListener(*loader);
  EXPECT_EQ(1U, manager_->listeners().size());
}

TEST_F(ListenerManagerImplWithRealFiltersTest, DefaultListenerPerConnectionBufferLimit) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": []
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  manager_->addOrUpdateListener(*loader);
  EXPECT_EQ(1024 * 1024U, manager_->listeners().back().get().perConnectionBufferLimitBytes());
}

TEST_F(ListenerManagerImplWithRealFiltersTest, SetListenerPerConnectionBufferLimit) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [],
    "per_connection_buffer_limit_bytes": 8192
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  manager_->addOrUpdateListener(*loader);
  EXPECT_EQ(8192U, manager_->listeners().back().get().perConnectionBufferLimitBytes());
}

TEST_F(ListenerManagerImplWithRealFiltersTest, SslContext) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters" : [],
    "ssl_context" : {
      "cert_chain_file" : "{{ test_rundir }}/test/common/ssl/test_data/san_uri_cert.pem",
      "private_key_file" : "{{ test_rundir }}/test/common/ssl/test_data/san_uri_key.pem",
      "verify_subject_alt_name" : [
        "localhost",
        "127.0.0.1"
      ]
    }
  }
  )EOF";

  Json::ObjectSharedPtr loader = TestEnvironment::jsonLoadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  manager_->addOrUpdateListener(*loader);
  EXPECT_NE(nullptr, manager_->listeners().back().get().sslContext());
}

TEST_F(ListenerManagerImplWithRealFiltersTest, BadListenerConfig) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [],
    "test": "a"
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_THROW(manager_->addOrUpdateListener(*loader), Json::Exception);
}

TEST_F(ListenerManagerImplWithRealFiltersTest, BadFilterConfig) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [
      {
        "type" : "type",
        "name" : "name",
        "config" : {}
      }
    ]
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_THROW(manager_->addOrUpdateListener(*loader), Json::Exception);
}

TEST_F(ListenerManagerImplWithRealFiltersTest, BadFilterName) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [
      {
        "type" : "write",
        "name" : "invalid",
        "config" : {}
      }
    ]
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_THROW_WITH_MESSAGE(manager_->addOrUpdateListener(*loader), EnvoyException,
                            "unable to create filter factory for 'invalid'/'write'");
}

TEST_F(ListenerManagerImplWithRealFiltersTest, BadFilterType) {
  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [
      {
        "type" : "write",
        "name" : "echo",
        "config" : {}
      }
    ]
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_THROW_WITH_MESSAGE(manager_->addOrUpdateListener(*loader), EnvoyException,
                            "unable to create filter factory for 'echo'/'write'");
}

class TestStatsConfigFactory : public Configuration::NamedNetworkFilterConfigFactory {
public:
  // Server::Configuration::NamedNetworkFilterConfigFactory
  Configuration::NetworkFilterFactoryCb
  createFilterFactory(const Json::Object&, Configuration::FactoryContext& context) override {
    context.scope().counter("bar").inc();
    return [](Network::FilterManager&) -> void {};
  }
  std::string name() override { return "stats_test"; }
  Configuration::NetworkFilterType type() override {
    return Configuration::NetworkFilterType::Read;
  }
};

TEST_F(ListenerManagerImplWithRealFiltersTest, StatsScopeTest) {
  Registry::RegisterFactory<TestStatsConfigFactory, Configuration::NamedNetworkFilterConfigFactory>
      registered;

  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "bind_to_port": false,
    "filters": [
      {
        "type" : "read",
        "name" : "stats_test",
        "config" : {}
      }
    ]
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, false));
  manager_->addOrUpdateListener(*loader);
  manager_->listeners().front().get().listenerScope().counter("foo").inc();

  EXPECT_EQ(1UL, server_.stats_store_.counter("bar").value());
  EXPECT_EQ(1UL, server_.stats_store_.counter("listener.127.0.0.1_1234.foo").value());
}

/**
 * Config registration for the echo filter using the deprecated registration class.
 */
class TestDeprecatedEchoConfigFactory : public Configuration::NetworkFilterConfigFactory {
public:
  // NetworkFilterConfigFactory
  Configuration::NetworkFilterFactoryCb
  tryCreateFilterFactory(Configuration::NetworkFilterType type, const std::string& name,
                         const Json::Object&, Server::Instance&) override {
    if (type != Configuration::NetworkFilterType::Read || name != "echo_deprecated") {
      return nullptr;
    }

    return [](Network::FilterManager&) -> void {};
  }
};

TEST_F(ListenerManagerImplWithRealFiltersTest, DeprecatedFilterConfigFactoryRegistrationTest) {
  // Test ensures that the deprecated network filter registration still works without error.

  // Register the config factory
  Configuration::RegisterNetworkFilterConfigFactory<TestDeprecatedEchoConfigFactory> registered;

  std::string json = R"EOF(
  {
    "address": "tcp://127.0.0.1:1234",
    "filters": [
      {
        "type" : "read",
        "name" : "echo_deprecated",
        "config" : {}
      }
    ]
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(json);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  manager_->addOrUpdateListener(*loader);
}

TEST_F(ListenerManagerImplTest, DynamicFlow) {
  InSequence s;

  std::string listener1 = R"EOF(
  {
    "name": "foo",
    "address": "tcp://127.0.0.1:1234",
    "filters": []
  }
  )EOF";

  Json::ObjectSharedPtr loader = Json::Factory::loadFromString(listener1);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_CALL(listener_factory_, createListenSocket(_, true));
  // Add first listener.
  EXPECT_TRUE(manager_->addOrUpdateListener(*loader));
  // Update duplicate should be a NOP.
  EXPECT_FALSE(manager_->addOrUpdateListener(*loader));

  std::string listener1_update1 = R"EOF(
  {
    "name": "foo",
    "address": "tcp://127.0.0.1:1234",
    "filters": [
      { "type" : "read", "name" : "fake", "config" : {} }
    ]
  }
  )EOF";

  loader = Json::Factory::loadFromString(listener1_update1);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  // Update first listener. Should share socket.
  EXPECT_TRUE(manager_->addOrUpdateListener(*loader));

  // Start workers.
  EXPECT_CALL(*worker_, addListener(_));
  EXPECT_CALL(*worker_, start(_));
  manager_->startWorkers(guard_dog_);

  // Update duplicate should be a NOP.
  EXPECT_FALSE(manager_->addOrUpdateListener(*loader));

  // Update. Should go into warming.
  loader = Json::Factory::loadFromString(listener1);
  EXPECT_CALL(listener_factory_, createFilterFactoryList(_, _));
  EXPECT_TRUE(manager_->addOrUpdateListener(*loader));
}

} // Server
} // Envoy
