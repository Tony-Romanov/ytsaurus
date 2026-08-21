#include <ucp/api/ucp.h>
#include <uct/api/uct.h>

#include <library/cpp/testing/unittest/registar.h>

#include <cstring>

Y_UNIT_TEST_SUITE(TUcxSmokeTest)
{
    Y_UNIT_TEST(CanCreateContextAndWorker)
    {
        uct_component_h* components = nullptr;
        unsigned componentCount = 0;
        UNIT_ASSERT_VALUES_EQUAL(
            static_cast<int>(uct_query_components(&components, &componentCount)),
            static_cast<int>(UCS_OK));

        bool haveSelf = false;
        bool havePosix = false;
        bool haveSysv = false;
        bool haveTcp = false;
        for (unsigned index = 0; index < componentCount; ++index) {
            uct_component_attr_t attr = {};
            attr.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME;
            UNIT_ASSERT_VALUES_EQUAL(
                static_cast<int>(uct_component_query(components[index], &attr)),
                static_cast<int>(UCS_OK));
            haveSelf |= std::strcmp(attr.name, "self") == 0;
            havePosix |= std::strcmp(attr.name, "posix") == 0;
            haveSysv |= std::strcmp(attr.name, "sysv") == 0;
            haveTcp |= std::strcmp(attr.name, "tcp") == 0;
        }
        uct_release_component_list(components);

        UNIT_ASSERT(haveSelf);
        UNIT_ASSERT(havePosix);
        UNIT_ASSERT(haveSysv);
        UNIT_ASSERT(haveTcp);

        ucp_config_t* config = nullptr;
        UNIT_ASSERT_VALUES_EQUAL(
            static_cast<int>(ucp_config_read(nullptr, nullptr, &config)),
            static_cast<int>(UCS_OK));

        ucp_params_t contextParams = {};
        contextParams.field_mask = UCP_PARAM_FIELD_FEATURES;
        contextParams.features = UCP_FEATURE_TAG;

        ucp_context_h context = nullptr;
        auto status = ucp_init(&contextParams, config, &context);
        ucp_config_release(config);
        UNIT_ASSERT_VALUES_EQUAL(static_cast<int>(status), static_cast<int>(UCS_OK));

        ucp_worker_params_t workerParams = {};
        workerParams.field_mask = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
        workerParams.thread_mode = UCS_THREAD_MODE_MULTI;

        ucp_worker_h worker = nullptr;
        status = ucp_worker_create(context, &workerParams, &worker);
        UNIT_ASSERT_VALUES_EQUAL(static_cast<int>(status), static_cast<int>(UCS_OK));

        ucp_worker_attr_t workerAttr = {};
        workerAttr.field_mask = UCP_WORKER_ATTR_FIELD_THREAD_MODE;
        UNIT_ASSERT_VALUES_EQUAL(
            static_cast<int>(ucp_worker_query(worker, &workerAttr)),
            static_cast<int>(UCS_OK));
        UNIT_ASSERT_VALUES_EQUAL(
            static_cast<int>(workerAttr.thread_mode),
            static_cast<int>(UCS_THREAD_MODE_MULTI));

        ucp_worker_destroy(worker);
        ucp_cleanup(context);
    }
}
