/*
 * freelan - An open, multi-platform software to establish peer-to-peer virtual
 * private networks.
 *
 * Copyright (C) 2010-2011 Julien KAUFFMANN <julien.kauffmann@freelan.org>
 *
 * This file is part of freelan.
 *
 * freelan is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * freelan is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 *
 * If you intend to use freelan in a commercial software, please
 * contact me : we may arrange this for a small fee or no fee at all,
 * depending on the nature of your project.
 */

/**
 * \file cli.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief The Command-Line Interface program entry point.
 */

#include <iostream>
#include <cstdlib>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>

#include <cryptoplus/cryptoplus.hpp>
#include <cryptoplus/error/error_strings.hpp>

#include <freelan/freelan.hpp>
#include <freelan/logger_stream.hpp>

#include "common/tools.hpp"
#include "common/system.hpp"
#include "common/configuration_helper.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace fl = freelan;

std::vector<fs::path> get_configuration_files()
{
	std::vector<fs::path> configuration_files;

#ifdef WINDOWS
	configuration_files.push_back(get_home_directory() / "freelan.cfg");
	configuration_files.push_back(get_application_directory() / "freelan.cfg");
#else
	configuration_files.push_back(get_home_directory() / ".freelan/freelan.cfg");
	configuration_files.push_back(get_application_directory() / "freelan.cfg");
#endif

	return configuration_files;
}

void log_function(freelan::log_level level, const std::string& msg)
{
	std::cout << boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::local_time()) << " [" << log_level_to_string(level) << "] " << msg << std::endl;
}

bool parse_options(int argc, char** argv, fl::configuration& configuration, bool& debug)
{
	po::options_description generic_options("Generic options");
	generic_options.add_options()
	("help,h", "Produce help message.")
	("debug,d", "Enables debug output.")
	("configuration_file,c", po::value<std::string>(), "The configuration file to use")
	;

	po::options_description configuration_options("Configuration");
	configuration_options.add(get_fscp_options());
	configuration_options.add(get_security_options());
	configuration_options.add(get_tap_adapter_options());
	configuration_options.add(get_switch_options());

	po::options_description visible_options;
	visible_options.add(generic_options);
	visible_options.add(configuration_options);

	po::options_description all_options;
	all_options.add(generic_options);
	all_options.add(configuration_options);

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, all_options), vm);

	std::string configuration_file;

	if (vm.count("configuration_file"))
	{
		configuration_file = vm["configuration_file"].as<std::string>();
	}
	else
	{
		char* val = getenv("FREELAN_CONFIGURATION_FILE");

		if (val)
		{
			configuration_file = std::string(val);
		}
	}

	if (!configuration_file.empty())
	{
		std::cout << "Reading configuration file at: " << configuration_file << std::endl;

		std::ifstream ifs(configuration_file.c_str());

		if (!ifs)
		{
			throw po::reading_file(configuration_file.c_str());
		}

		po::store(po::parse_config_file(ifs, configuration_options, true), vm);
	}
	else
	{
		bool configuration_read = false;

		const std::vector<fs::path> configuration_files = get_configuration_files();

		BOOST_FOREACH(const fs::path& conf, configuration_files)
		{
			fs::basic_ifstream<char> ifs(conf);

			if (ifs)
			{
				std::cout << "Reading configuration file at: " << conf << std::endl;

				po::store(po::parse_config_file(ifs, configuration_options, true), vm);

				break;
			}
		}

		if (!configuration_read)
		{
			std::cerr << "Warning ! No configuration file specified and none found in the environment." << std::endl;
			std::cerr << "Looked up locations were:" << std::endl;

			BOOST_FOREACH(const fs::path& conf, configuration_files)
			{
				std::cerr << "- " << conf << std::endl;
			}
		}
	}

	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << visible_options << std::endl;

		return false;
	}

	setup_configuration(configuration, vm);

	const fs::path certificate_validation_script = get_certificate_validation_script(vm);

	if (!certificate_validation_script.empty())
	{
		configuration.security.certificate_validation_callback = boost::bind(&execute_certificate_validation_script, certificate_validation_script, _1, _2);
	}

	if (vm.count("debug"))
	{
		debug = true;
	}

	return true;
}

int main(int argc, char** argv)
{
	cryptoplus::crypto_initializer crypto_initializer;
	cryptoplus::algorithms_initializer algorithms_initializer;
	cryptoplus::error::error_strings_initializer error_strings_initializer;

	try
	{
		fl::configuration configuration;
		bool debug = false;

		if (parse_options(argc, argv, configuration, debug))
		{
			boost::asio::io_service io_service;

			freelan::core core(io_service, configuration, fl::logger(&log_function, debug ? fl::LOG_DEBUG : fl::LOG_INFORMATION));

			core.open();

			stop_function = boost::bind(&freelan::core::close, boost::ref(core));

			if (core.has_tap_adapter())
			{
				std::cout << "Using tap adapter: " << core.tap_adapter().name() << std::endl;
			}
			else
			{
				std::cout << "Configured not to use any tap adapter." << std::endl;
			}

			std::cout << "Listening on: " << core.server().socket().local_endpoint() << std::endl;

			io_service.run();

			stop_function = 0;
		}
	}
	catch (std::exception& ex)
	{
		std::cerr << "Error: " << ex.what() << std::endl;

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}