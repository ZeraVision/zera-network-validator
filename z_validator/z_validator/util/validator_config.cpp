#include "validators.h"
#include "wallets.h"
#include "../logging/logging.h"

std::string ValidatorConfig::host_;
std::string ValidatorConfig::client_port_;
std::string ValidatorConfig::validator_port_;
std::string ValidatorConfig::fee_address_string_;
std::vector<std::string> ValidatorConfig::seed_validators_;
std::vector<std::string> ValidatorConfig::staked_contract_ids_;
uint32_t ValidatorConfig::version_;
KeyPair ValidatorConfig::key_pair_;
KeyPair ValidatorConfig::gen_key_pair_;
std::vector<uint8_t> ValidatorConfig::fee_address_;
std::string ValidatorConfig::block_height_;
std::mutex ValidatorConfig::mutex_;
bool ValidatorConfig::dev_mode_ = false;       //by default dev mode is set to true
std::string ValidatorConfig::treasury_wallet_;
std::string ValidatorConfig::register_;

void ValidatorConfig::generate_keys()
{
	std::string wallet_type = "A_a_c_";
	gen_key_pair_ = wallets::generate_key_pair(KeyType::ED25519);
	std::string pub_key_string(gen_key_pair_.public_key.begin(), gen_key_pair_.public_key.end());
	pub_key_string = wallet_type + pub_key_string;
	std::vector<uint8_t> pub_key_vec(pub_key_string.begin(), pub_key_string.end());
	gen_key_pair_.public_key = pub_key_vec;
}
void ValidatorConfig::set_configs(const std::string& line)
{
	std::size_t delimiterPos = line.find(':');

	if (delimiterPos != std::string::npos)
	{
		std::string key = line.substr(0, delimiterPos);
		std::string value = line.substr(delimiterPos + 1);

		// Process the key-value pair as needed
		if (key == "host")
		{
			ValidatorConfig::set_host(value);
		}
		else if (key == "client_port")
		{
			ValidatorConfig::set_client_port(value);
		}
		else if (key == "server_port")
		{
			ValidatorConfig::set_validator_port(value);
		}
		else if (key == "public_key")
		{
			ValidatorConfig::set_public_key(value);
		}
		else if (key == "private_key")
		{
			ValidatorConfig::set_private_key(value);
		}
		else if (key == "seed_validator")
		{
			ValidatorConfig::set_seed_validators(value);
		}
		else if (key == "fee_wallet_address")
		{
			ValidatorConfig::set_fee_address(value);
		}
		else if (key == "staked_contract_id")
		{
			ValidatorConfig::set_staked_contract_id(value);
		}
		else if (key == "block_height")
		{
			ValidatorConfig::set_block_height(value);
		}
		else if (key == "dev_mode")
		{
			bool dev = false;
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);

			if(value == "true" || value == "yes")
			{
				dev = true;
			}

			ValidatorConfig::set_dev_mode(dev);
		}
		else if(key == "register")
		{
			std::transform(value.begin(), value.end(), value.begin(), ::tolower);

			if(value == "true" || value == "false")
			{
				ValidatorConfig::set_register(value);
			}
			else
			{
				ValidatorConfig::set_register("N/A");
			}
		}
	}
}
void ValidatorConfig::process_config_file(std::ifstream &configFile)
{
	std::string line;
	while (std::getline(configFile, line))
	{
		set_configs(line);
	}
}

void ValidatorConfig::copy_and_process_config_file(std::ifstream &srcFile, std::ofstream &destFile)
{
	std::string line;
	while (std::getline(srcFile, line))
	{
		if (!(destFile << line << '\n'))
		{
			std::cerr << "Failed to write to destination config file." << std::endl;
		}
		set_configs(line);
	}
}

void ValidatorConfig::set_config()
{
	std::ifstream volume_config(VALIDATOR_CONFIG);
	std::ifstream file("/z_validator/config/validator.conf");
	set_version(VERSION);
	set_treasury_wallet(TREASURY_WALLET);

	if (volume_config.is_open())
	{
		process_config_file(volume_config);
		volume_config.close();
	}
	else if (file.is_open())
	{
		std::ofstream destFile(VALIDATOR_CONFIG);
		if (!destFile.is_open())
		{
			logging::print("Failed to open the destination config file.", false);
		}
		else
		{
			copy_and_process_config_file(file, destFile);
			file.close();
			destFile.close(); // Close the destination file
		}
	}
	else
	{
		logging::print("Failed to open validator config file.", false);
	}
}

void ValidatorConfig::set_fee_address()
{
	std::ifstream file(VALIDATOR_CONFIG);
	if (file.is_open())
	{
		std::string line;

		while (std::getline(file, line))
		{
			std::size_t delimiterPos = line.find(':');
			if (delimiterPos != std::string::npos)
			{
				std::string key = line.substr(0, delimiterPos);
				std::string value = line.substr(delimiterPos + 1);
				// Process the key-value pair as needed
				if (key == "fee_wallet_address")
				{
					ValidatorConfig::set_fee_address(value);
				}
			}
			file.close(); // Close the file
		}
	}
	else
	{
		logging::print("Failed to open validator config file.", false);
	}
}

uint32_t ValidatorConfig::get_version()
{
	return version_;
}
std::string ValidatorConfig::get_host()
{
	return host_;
}
std::string ValidatorConfig::get_client_port()
{
	return client_port_;
}
std::string ValidatorConfig::get_validator_port()
{
	return validator_port_;
}
std::vector<std::string> ValidatorConfig::get_seed_validators()
{
	return seed_validators_;
}

KeyPair ValidatorConfig::get_gen_key_pair()
{
	return gen_key_pair_;
}
std::string ValidatorConfig::get_gen_public_key()
{
	std::string public_key(gen_key_pair_.public_key.begin(), gen_key_pair_.public_key.end());
	return public_key;
}
std::string ValidatorConfig::get_gen_private_key()
{
	std::string private_key(gen_key_pair_.private_key.begin(), gen_key_pair_.private_key.end());
	return private_key;
}

KeyPair ValidatorConfig::get_key_pair()
{
	return key_pair_;
}
std::string ValidatorConfig::get_public_key()
{
	std::string public_key(key_pair_.public_key.begin(), key_pair_.public_key.end());
	return public_key;
}
std::string ValidatorConfig::get_private_key()
{
	std::string private_key(key_pair_.private_key.begin(), key_pair_.private_key.end());
	return private_key;
}
std::string ValidatorConfig::get_block_height()
{
	return block_height_;
}
std::vector<uint8_t> ValidatorConfig::get_fee_address()
{
	return fee_address_;
}
std::string ValidatorConfig::get_fee_address_string()
{
	return fee_address_string_;
}
std::vector<std::string> ValidatorConfig::get_staked_contract_id()
{
	return staked_contract_ids_;

}
std::string ValidatorConfig::get_treasury_wallet()
{
	return treasury_wallet_;
}

bool ValidatorConfig::get_dev_mode()
{
	return dev_mode_;
}
std::string ValidatorConfig::get_register()
{
	return register_;
}

void ValidatorConfig::set_register(const std::string& register_string)
{
	std::lock_guard<std::mutex> lock(mutex_);
	register_ = register_string;
}
void ValidatorConfig::set_block_height(const std::string& block_height)
{
	std::lock_guard<std::mutex> lock(mutex_);
	block_height_ = block_height;
}

void ValidatorConfig::set_staked_contract_id(const std::string& staked_contract_id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (std::find(staked_contract_ids_.begin(), staked_contract_ids_.end(), staked_contract_id) == staked_contract_ids_.end())
	{
		staked_contract_ids_.push_back(staked_contract_id);
	}

	staked_contract_ids_.push_back(staked_contract_id);
}

void ValidatorConfig::set_host(const std::string& host)
{
	std::lock_guard<std::mutex> lock(mutex_);
	host_ = host;
}
void ValidatorConfig::set_client_port(const std::string& client_port)
{
	std::lock_guard<std::mutex> lock(mutex_);
	client_port_ = client_port;
}
void ValidatorConfig::set_validator_port(const std::string& validator_port)
{
	std::lock_guard<std::mutex> lock(mutex_);
	validator_port_ = validator_port;
}
void ValidatorConfig::set_seed_validators(const std::string& seed_validator)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (seed_validator != "")
	{
		seed_validators_.push_back(seed_validator);
	}
}

void ValidatorConfig::set_public_key(const std::string& public_key)
{
	std::lock_guard<std::mutex> lock(mutex_);
	key_pair_.public_key = base58_decode_public_key(public_key);
}
void ValidatorConfig::set_private_key(const std::string& private_key)
{
	std::lock_guard<std::mutex> lock(mutex_);
	key_pair_.private_key = base58_decode(private_key);
}
void ValidatorConfig::set_fee_address(const std::string& fee_address)
{
	fee_address_ = base58_decode(fee_address);
	set_fee_address_string(fee_address_);
}

void ValidatorConfig::set_fee_address_string(const std::vector<uint8_t>& fee_address)
{
	std::string temp(fee_address.begin(), fee_address.end());
	fee_address_string_ = temp;
}

void ValidatorConfig::set_version(const uint32_t& version)
{
	version_ = version;
}

void ValidatorConfig::set_dev_mode(bool dev_mode)
{
	dev_mode_ = dev_mode;
}

void ValidatorConfig::set_treasury_wallet(const std::string& treasury_wallet)
{
	treasury_wallet_ = treasury_wallet;
}