//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm@gmail.com
//

#pragma once

#include <boost/thread.hpp>

#include "internal.hpp"
#include "io_service_pool.hpp"
#include "http_connection.hpp"

#include <boost/noncopyable.hpp>

namespace av_router {

	typedef boost::function<void(const request&, http_connection_ptr, http_connection_manager&)> http_request_callback;
	typedef std::map<std::string, http_request_callback> http_request_callback_table;

	class http_connection;
	class http_server
		: public boost::noncopyable
	{
		friend class http_connection;
	public:
		explicit http_server(io_service_pool& ios, unsigned short port, std::string address = "0.0.0.0");
		~http_server();

	public:
		void start();
		void stop();

		bool add_uri_handler(const std::string& uri, http_request_callback);

	private:
		void handle_accept(const boost::system::error_code& error);
		void on_tick(const boost::system::error_code& error);

		// 收到一个 http request 的时候调用
		bool handle_request(const request&, http_connection_ptr);

	private:
		io_service_pool& m_io_service_pool;
		boost::asio::io_service& m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		bool m_listening;
		http_connection_ptr m_connection;
		http_connection_manager m_connection_manager;
		boost::asio::deadline_timer m_timer;
		boost::shared_mutex m_request_callback_mtx;
		http_request_callback_table m_http_request_callbacks;
	};

}
