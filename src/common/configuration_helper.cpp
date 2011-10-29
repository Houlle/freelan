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
 * \file configuration_helper.cpp
 * \author Julien KAUFFMANN <julien.kauffmann@freelan.org>
 * \brief A configuration helper.
 */

#include "configuration_helper.hpp"

#include <vector>

#include <boost/asio.hpp>
#include <boost/foreach.hpp>

#include "parsers.hpp"

namespace po = boost::program_options;
namespace fl = freelan;

namespace
{
	fl::fscp_configuration::hostname_resolution_protocol_type parse_network_hostname_resolution_protocol(const std::string& str)
	{
		if (str == "system_default")
			return boost::asio::ip::udp::v4();
		else if (str == "ipv4")
			return boost::asio::ip::udp::v4();
		else if (str == "ipv6")
			return boost::asio::ip::udp::v6();

		throw std::runtime_error("\"" + str + "\" is not a valid hostname resolution protocol");
	}

	fl::security_configuration::certificate_validation_method_type to_certificate_validation_method(const std::string& str)
	{
		if (str == "default")
			return fl::security_configuration::CVM_DEFAULT;
		if (str == "none")
			return fl::security_configuration::CVM_NONE;

		throw std::runtime_error("\"" + str + "\" is not a valid certificate validation method");
	}

	boost::posix_time::time_duration to_time_duration(unsigned int msduration)
	{
		return boost::posix_time::milliseconds(msduration);
	}

	cryptoplus::file load_file(const std::string& filename)
	{
		try
		{
			return cryptoplus::file::open(filename);
		}
		catch (std::runtime_error&)
		{
			throw std::runtime_error("Unable to open the specified file: " + filename);
		}
	}

	fl::security_configuration::cert_type load_certificate(const std::string& filename)
	{
		return fl::security_configuration::cert_type::from_certificate(load_file(filename));
	}

	cryptoplus::pkey::pkey load_private_key(const std::string& filename)
	{
		return cryptoplus::pkey::pkey::from_private_key(load_file(filename));
	}

	fl::security_configuration::cert_type load_trusted_certificate(const std::string& filename)
	{
		return fl::security_configuration::cert_type::from_trusted_certificate(load_file(filename));
	}

	fl::switch_configuration::routing_method_type to_routing_method(const std::string& str)
	{
		if (str == "switch")
			return fl::switch_configuration::RM_SWITCH;
		if (str == "hub")
			return fl::switch_configuration::RM_HUB;

		throw std::runtime_error("\"" + str + "\" is not a valid routing method");
	}
}

po::options_description get_fscp_options()
{
	po::options_description result("FreeLAN Secure Channel Protocol (FSCP) options");

	result.add_options()
	("fscp.hostname_resolution_protocol", po::value<std::string>()->default_value("system_default"), "The hostname resolution protocol to use.")
	("fscp.listen_on", po::value<std::string>()->default_value("0.0.0.0:12000"), "The endpoint to listen on.")
	("fscp.hello_timeout", po::value<unsigned int>()->default_value(3000, "3000"), "The default timeout for HELLO messages, in milliseconds.")
	("fscp.contact", po::value<std::vector<std::string> >()->multitoken()->zero_tokens()->default_value(std::vector<std::string>(), ""), "The address of an host to contact.")
	;

	return result;
}

po::options_description get_security_options()
{
	po::options_description result("Security options");

	result.add_options()
	("security.signature_certificate_file", po::value<std::string>()->required(), "The certificate file to use for signing.")
	("security.signature_private_key_file", po::value<std::string>()->required(), "The private key file to use for signing.")
	("security.encryption_certificate_file", po::value<std::string>(), "The certificate file to use for encryption.")
	("security.encryption_private_key_file", po::value<std::string>(), "The private key file to use for encryption.")
	("security.certificate_validation_method", po::value<std::string>()->default_value("default"), "The certificate validation method.")
	("security.certificate_validation_script", po::value<std::string>(), "The certificate validation script to use.")
	("security.authority_certificate_file", po::value<std::vector<std::string> >()->multitoken()->zero_tokens()->default_value(std::vector<std::string>(), ""), "An authority certificate file to use.")
	;

	return result;
}

po::options_description get_tap_adapter_options()
{
	po::options_description result("Tap adapter options");

	result.add_options()
	("tap_adapter.enabled", po::value<bool>()->default_value(true, "yes"), "Whether to enable the tap adapter.")
	("tap_adapter.ipv4_address_prefix_length", po::value<std::string>()->default_value("9.0.0.1/24"), "The tap adapter IPv4 address and prefix length.")
	("tap_adapter.ipv6_address_prefix_length", po::value<std::string>()->default_value("fe80::1/10"), "The tap adapter IPv6 address and prefix length.")
	("tap_adapter.arp_proxy_enabled", po::value<bool>()->default_value(false), "Whether to enable the ARP proxy.")
	("tap_adapter.arp_proxy_fake_ethernet_address", po::value<std::string>()->default_value("00:aa:bb:cc:dd:ee"), "The ARP proxy fake ethernet address.")
	("tap_adapter.dhcp_proxy_enabled", po::value<bool>()->default_value(true), "Whether to enable the DHCP proxy.")
	("tap_adapter.dhcp_server_ipv4_address_prefix_length", po::value<std::string>()->default_value("9.0.0.0/24"), "The DHCP proxy server IPv4 address and prefix length.")
	("tap_adapter.dhcp_server_ipv6_address_prefix_length", po::value<std::string>()->default_value("fe80::/10"), "The DHCP proxy server IPv6 address and prefix length.")
	;

	return result;
}

po::options_description get_switch_options()
{
	po::options_description result("Switch options");

	result.add_options()
	("switch.routing_method", po::value<std::string>()->default_value("switch"), "The routing method for messages.")
	("switch.relay_mode_enabled", po::value<bool>()->default_value(false, "no"), "Whether to enable the relay mode.")
	;

	return result;
}

void setup_configuration(fl::configuration& configuration, const po::variables_map& vm)
{
	typedef boost::asio::ip::udp::resolver::query query;
	typedef fl::security_configuration::cert_type cert_type;
	typedef cryptoplus::pkey::pkey pkey;

	// FSCP options
	configuration.fscp.hostname_resolution_protocol = parse_network_hostname_resolution_protocol(vm["fscp.hostname_resolution_protocol"].as<std::string>());
	configuration.fscp.listen_on = parse<fl::fscp_configuration::ep_type>(vm["fscp.listen_on"].as<std::string>());
	configuration.fscp.hello_timeout = to_time_duration(vm["fscp.hello_timeout"].as<unsigned int>());

	const std::vector<std::string> contact_list = vm["fscp.contact"].as<std::vector<std::string> >();

	configuration.fscp.contact_list.clear();

	BOOST_FOREACH(const std::string& contact, contact_list)
	{
		configuration.fscp.contact_list.push_back(parse<fl::fscp_configuration::ep_type>(contact));
	}

	// Security options
	cert_type signature_certificate = load_certificate(vm["security.signature_certificate_file"].as<std::string>());
	pkey signature_private_key = load_private_key(vm["security.signature_private_key_file"].as<std::string>());

	cert_type encryption_certificate;
	pkey encryption_private_key;

	if (vm.count("security.encryption_certificate_file"))
	{
		encryption_certificate = load_certificate(vm["security.encryption_certificate_file"].as<std::string>());
	}

	if (vm.count("security.encryption_private_key_file"))
	{
		encryption_private_key = load_private_key(vm["security.encryption_private_key_file"].as<std::string>());
	}

	configuration.security.identity = fscp::identity_store(signature_certificate, signature_private_key, encryption_certificate, encryption_private_key);

	configuration.security.certificate_validation_method = to_certificate_validation_method(vm["security.certificate_validation_method"].as<std::string>());

	const std::vector<std::string> authority_certificate_file_list = vm["security.authority_certificate_file"].as<std::vector<std::string> >();

	configuration.security.certificate_authority_list.clear();

	BOOST_FOREACH(const std::string& authority_certificate_file, authority_certificate_file_list)
	{
		configuration.security.certificate_authority_list.push_back(load_trusted_certificate(authority_certificate_file));
	}

	// Tap adapter options
	configuration.tap_adapter.enabled = vm["tap_adapter.enabled"].as<bool>();
	configuration.tap_adapter.ipv4_address_prefix_length = parse_optional<fl::tap_adapter_configuration::ipv4_address_prefix_length_type>(vm["tap_adapter.ipv4_address_prefix_length"].as<std::string>());
	configuration.tap_adapter.ipv6_address_prefix_length = parse_optional<fl::tap_adapter_configuration::ipv6_address_prefix_length_type>(vm["tap_adapter.ipv6_address_prefix_length"].as<std::string>());
	configuration.tap_adapter.arp_proxy_enabled = vm["tap_adapter.arp_proxy_enabled"].as<bool>();
	configuration.tap_adapter.arp_proxy_fake_ethernet_address = parse<fl::tap_adapter_configuration::ethernet_address_type>(vm["tap_adapter.arp_proxy_fake_ethernet_address"].as<std::string>());
	configuration.tap_adapter.dhcp_proxy_enabled = vm["tap_adapter.dhcp_proxy_enabled"].as<bool>();
	configuration.tap_adapter.dhcp_server_ipv4_address_prefix_length = parse_optional<fl::tap_adapter_configuration::ipv4_address_prefix_length_type>(vm["tap_adapter.dhcp_server_ipv4_address_prefix_length"].as<std::string>());
	configuration.tap_adapter.dhcp_server_ipv6_address_prefix_length = parse_optional<fl::tap_adapter_configuration::ipv6_address_prefix_length_type>(vm["tap_adapter.dhcp_server_ipv6_address_prefix_length"].as<std::string>());

	// Switch options
	configuration.switch_.routing_method = to_routing_method(vm["switch.routing_method"].as<std::string>());
	configuration.switch_.relay_mode_enabled = vm["switch.relay_mode_enabled"].as<bool>();
}

std::string get_certificate_validation_script(const boost::program_options::variables_map& vm)
{
	return vm["security.certificate_validation_script"].as<std::string>();
}