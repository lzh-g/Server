#ifndef _HTTPSTATUSCODE_H_
#define _HTTPSTATUSCODE_H_
#include "stdafx.h"
#include <iostream>
#include <string>
#include <unordered_map>

#define CONNFD_ET // 边缘触发非阻塞
// #define CONNFD_LT//水平触发阻塞

#define LISTENFD_ET // 边缘触发非阻塞
// #define LISTENFD_LT//水平触发阻塞

const char *doc_root = "/home/lzh/project/Server/include/Root";

enum HTTP_LIB HttpStatusCode
{
    success_ok = 200,

    error_bad_request = 400,
    error_forbidden = 403,
    error_not_found = 404,

    error_internal_error = 500

};
// HTTP响应状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 报文请求方法
enum HTTP_LIB METHOD
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
};

// 主状态机的状态
enum HTTP_LIB CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0, // 解析请求行
    CHECK_STATE_HEADER,          // 解析请求头
    CHECK_STATE_CONTENT          // 解析消息体，仅用于解析POST请求
};

// 报文解析结果
enum HTTP_LIB HTTP_CODE
{
    NO_REQUEST,        // 请求不完成，需要继续读取请求报文数据
    GET_REQUEST,       // 获得了完成的HTTP请求
    BAD_REQUEST,       // HTTP请求报文有语法错误
    NO_RESOURCE,       //
    FORBIDDEN_REQUEST, //
    FILE_REQUEST,      //
    INTERNAL_ERROR,    // 服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
    CLOSE_CONNECTION   //
};

// 从状态机的状态
enum HTTP_LIB LINE_STATUS
{
    LINE_OK = 0, // 完整读取一行
    LINE_BAD,    // 报文语法有误
    LINE_OPEN    // 读取行不完整
};

inline HTTP_LIB const std::unordered_map<HttpStatusCode, std::string> &StatusCodeString()
{
    static const std::unordered_map<HttpStatusCode, std::string> status_code_string =
        {
            {HttpStatusCode::success_ok, "200 OK"},

            {HttpStatusCode::error_bad_request, "400 Bad Request"},
            {HttpStatusCode::error_forbidden, "403 Forbidden"},
            {HttpStatusCode::error_not_found, "404 Not Found"},

            {HttpStatusCode::error_internal_error, "Internal Error"}};
}
#endif //!_HTTPSTATUSCODE_H_