﻿// Copyright 2022 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#include "errors.h"
#include <cassert>
#include <cerrno>
#include <unordered_map>
#include <tuple>
#include "shards.h"

namespace {

using namespace sw::redis;

std::pair<ReplyErrorType, std::string> parse_error(const std::string &msg);

std::unordered_map<std::string, ReplyErrorType> error_map = {
    {"MOVED", ReplyErrorType::MOVED},
    {"ASK", ReplyErrorType::ASK}
};

}

namespace sw {

namespace redis {

void throw_error(const redisContext &context, const std::string &err_info) {
    auto err_code = context.err;
    const auto *err_str = context.errstr;
    if (err_str == nullptr) {
        throw Error(err_info + ": null error message: " + std::to_string(err_code));
    }

    auto err_msg = err_info + ": " + err_str;

    switch (err_code) {
    case REDIS_ERR_IO:
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            throw TimeoutError(err_msg);
        } else {
            throw IoError(err_msg);
        }
        break;

    case REDIS_ERR_EOF:
        throw ClosedError(err_msg);
        break;

    case REDIS_ERR_PROTOCOL:
        throw ProtoError(err_msg);
        break;

    case REDIS_ERR_OOM:
        throw OomError(err_msg);
        break;

    case REDIS_ERR_OTHER:
        throw Error(err_msg);
        break;

#ifdef REDIS_ERR_TIMEOUT
    case REDIS_ERR_TIMEOUT:
        throw TimeoutError(err_msg);
        break;
#endif

    default:
        throw Error("unknown error code: " + err_msg);
    }
}

void throw_error(const redisReply &reply) {
    assert(reply.type == REDIS_REPLY_ERROR);

    if (reply.str == nullptr) {
        throw Error("Null error reply");
    }

    auto err_str = std::string(reply.str, reply.len);

    auto err_type = ReplyErrorType::ERR;
    std::string err_msg;
    std::tie(err_type, err_msg) = parse_error(err_str);

    switch (err_type) {
    case ReplyErrorType::MOVED:
        throw MovedError(err_msg);
        break;

    case ReplyErrorType::ASK:
        throw AskError(err_msg);
        break;

    default:
        throw ReplyError(err_str);
        break;
    }
}

}

}

namespace {

using namespace sw::redis;

std::pair<ReplyErrorType, std::string> parse_error(const std::string &err) {
    // The error contains an Error Prefix, and an optional error message.
    auto idx = err.find_first_of(" \n");

    if (idx == std::string::npos) {
        throw ProtoError("No Error Prefix: " + err);
    }

    auto err_prefix = err.substr(0, idx);
    auto err_type = ReplyErrorType::ERR;

    auto iter = error_map.find(err_prefix);
    if (iter != error_map.end()) {
        // Specific error.
        err_type = iter->second;
    } // else Generic error.

    return {err_type, err.substr(idx + 1)};
}

}
