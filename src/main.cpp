#include <iostream>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "include/io_service_pool.hpp"
#include "include/http_server.hpp"

using namespace av_router;

int main(int argc, char** argv)
{
	try
	{
		unsigned short http_port = 0;

		int num_threads = 0;
		int pool_size = 0;

		int db_port = 0;
		std::string db_host;
		std::string db_user_name;
		std::string db_password;

		po::options_description desc("options");
		desc.add_options()
			("help,h", "help message")
			("version", "current avrouter version")

			("httpport", po::value<unsigned short>(&http_port)->default_value(9999), "http RPC listen port")
			("thread", po::value<int>(&num_threads)->default_value(boost::thread::hardware_concurrency()), "threads")
			("pool", po::value<int>(&pool_size)->default_value(16), "connection pool size")

			("db_host", po::value<std::string>(&db_host)->default_value("tcp://192.168.1.254:3306/zhushou_test"), "connection data base host")
			("db_user_name", po::value<std::string>(&db_user_name)->default_value("root"), "connection data base user name")
			("db_password", po::value<std::string>(&db_password)->default_value(""), "connection data base password")
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.count("help"))
		{
			std::cout << desc << "\n";
			return 0;
		}

		// 指定线程并发数.
		io_service_pool io_pool(num_threads);
		// 创建 http 服务器.
		http_server http_serv(io_pool, http_port);

		// http_serv.add_uri_handler("test", nullptr);

		// 启动 HTTPD.
		LOG_DBG << "start httpd";
		http_serv.start();
		
		// 开始启动整个系统事件循环.
		io_pool.run();
	}
	catch (std::exception& e)
	{
		LOG_ERR << "main exception: " << e.what();
		return -1;
	}
	return 0;
}
