/*
 * Copyright 2017-2019 Baidu Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "openrasp_hook.h"
#include "openrasp_v8.h"

/**
 * mongo相关hook点
 */
PRE_HOOK_FUNCTION_EX(insert, mongodb_0_driver_0_bulkwrite, MONGO);
PRE_HOOK_FUNCTION_EX(update, mongodb_0_driver_0_bulkwrite, MONGO);
PRE_HOOK_FUNCTION_EX(__construct, mongodb_0_driver_0_query, MONGO);
PRE_HOOK_FUNCTION_EX(__construct, mongodb_0_driver_0_command, MONGO);

static void mongo_plugin_check(const std::string &query_str, const std::string &classname, const std::string &method)
{
    openrasp::Isolate *isolate = OPENRASP_V8_G(isolate);
    if (isolate && !query_str.empty() && !classname.empty() && !method.empty())
    {
        std::string cache_key = std::string(get_check_type_name(MONGO)).append(classname).append(method).append(query_str);
        if (OPENRASP_HOOK_G(lru).contains(cache_key))
        {
            return;
        }
        bool is_block = false;
        {
            v8::HandleScope handle_scope(isolate);
            auto params = v8::Object::New(isolate);
            params->Set(openrasp::NewV8String(isolate, "query"), openrasp::NewV8String(isolate, query_str));
            params->Set(openrasp::NewV8String(isolate, "class"), openrasp::NewV8String(isolate, classname));
            params->Set(openrasp::NewV8String(isolate, "method"), openrasp::NewV8String(isolate, method));
            params->Set(openrasp::NewV8String(isolate, "server"), openrasp::NewV8String(isolate, "mongo"));
            is_block = isolate->Check(openrasp::NewV8String(isolate, get_check_type_name(MONGO)), params, OPENRASP_CONFIG(plugin.timeout.millis));
        }
        if (is_block)
        {
            handle_block(TSRMLS_C);
        }
        OPENRASP_HOOK_G(lru).set(cache_key, true);
    }
}

//for mongodb extension
static void mongodb_plugin_check(zval *query, const std::string &classname, const std::string &method)
{
    std::string query_json;
    if (Z_TYPE_P(query) == IS_ARRAY)
    {
        query_json = json_encode_from_zval(query);
    }
    else if (Z_TYPE_P(query) == IS_OBJECT)
    {
        zval fromphp, serialized_bson;
        ZVAL_STRING(&fromphp, "mongodb\\bson\\fromphp");
        if (call_user_function(EG(function_table), nullptr, &fromphp, &serialized_bson, 1, query) == SUCCESS)
        {
            if (Z_TYPE(serialized_bson) == IS_STRING)
            {
                zval tojson, serialized_json;
                ZVAL_STRING(&tojson, "mongodb\\bson\\tojson");
                if (call_user_function(EG(function_table), nullptr, &tojson, &serialized_json, 1, &serialized_bson) == SUCCESS)
                {
                    if (Z_TYPE(serialized_json) == IS_STRING)
                    {
                        query_json = std::string(Z_STRVAL(serialized_json));
                    }
                    zval_ptr_dtor(&serialized_json);
                }
                zval_ptr_dtor(&tojson);
            }
            zval_ptr_dtor(&serialized_bson);
        }
        zval_ptr_dtor(&fromphp);
    }
    mongo_plugin_check(query_json, classname, method);
}

void pre_mongodb_0_driver_0_bulkwrite_insert_MONGO(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    zval *zquery, *zoptions = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "A|a!", &zquery, &zoptions) == FAILURE)
    {
        return;
    }
    mongodb_plugin_check(zquery, "MongoDB\\Driver\\Bulkwrite", "dalete");
}

void pre_mongodb_0_driver_0_bulkwrite_update_MONGO(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    zval *zquery, *zupdate, *zoptions = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "AA|a!", &zquery, &zupdate, &zoptions) == FAILURE)
    {
        return;
    }

    mongodb_plugin_check(zquery, "MongoDB\\Driver\\Bulkwrite", "update");
}

void pre_mongodb_0_driver_0_query___construct_MONGO(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    zval *filter;
    zval *options = NULL;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "A|a!", &filter, &options) == FAILURE)
    {
        return;
    }
    mongodb_plugin_check(filter, "MongoDB\\Driver\\Query", "__construct");
}

void pre_mongodb_0_driver_0_command___construct_MONGO(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    zval *document;
    zval *options = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "A|a!", &document, &options) == FAILURE)
    {
        return;
    }

    mongodb_plugin_check(document, "MongoDB\\Driver\\Command", "__construct");
}