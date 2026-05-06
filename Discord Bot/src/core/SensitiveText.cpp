#include "core/SensitiveText.hpp"
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <ntstatus.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace {
	constexpr const char* PREFIX = "enc:v1:";
	constexpr std::size_t PREFIX_SIZE = 7;
	constexpr ULONG NONCE_SIZE = 12;
	constexpr ULONG TAG_SIZE = 16;

	std::string env_value(const char* name) {
		char* value = nullptr;
		size_t size = 0;
		const errno_t rc = _dupenv_s(&value, &size, name);
		std::string result;
		if (rc == 0 && value) {
			result = value;
		}
		free(value);
		return result;
	}

	bool nt_ok(NTSTATUS status) {
		return status >= 0;
	}

	std::vector<unsigned char> random_bytes(ULONG size) {
		std::vector<unsigned char> out(size);
		if (!nt_ok(BCryptGenRandom(nullptr, out.data(), size, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
			throw std::runtime_error("Could not generate random bytes.");
		}
		return out;
	}

	std::vector<unsigned char> sha256(const std::string& value) {
		BCRYPT_ALG_HANDLE alg = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		DWORD object_size = 0;
		DWORD data_size = 0;
		std::vector<unsigned char> object;
		std::vector<unsigned char> digest(32);

		if (!nt_ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
			throw std::runtime_error("Could not open SHA-256 provider.");
		}

		if (!nt_ok(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &data_size, 0))) {
			BCryptCloseAlgorithmProvider(alg, 0);
			throw std::runtime_error("Could not read hash object size.");
		}

		object.resize(object_size);
		if (!nt_ok(BCryptCreateHash(alg, &hash, object.data(), object_size, nullptr, 0, 0))
			|| !nt_ok(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())), static_cast<ULONG>(value.size()), 0))
			|| !nt_ok(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0))) {
			if (hash) BCryptDestroyHash(hash);
			BCryptCloseAlgorithmProvider(alg, 0);
			throw std::runtime_error("Could not hash data key.");
		}

		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(alg, 0);
		return digest;
	}

	std::string base64_encode(const std::vector<unsigned char>& bytes) {
		static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((bytes.size() + 2) / 3) * 4);

		for (std::size_t i = 0; i < bytes.size(); i += 3) {
			const unsigned int b0 = bytes[i];
			const unsigned int b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
			const unsigned int b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
			const unsigned int triple = (b0 << 16) | (b1 << 8) | b2;

			out.push_back(alphabet[(triple >> 18) & 0x3f]);
			out.push_back(alphabet[(triple >> 12) & 0x3f]);
			out.push_back(i + 1 < bytes.size() ? alphabet[(triple >> 6) & 0x3f] : '=');
			out.push_back(i + 2 < bytes.size() ? alphabet[triple & 0x3f] : '=');
		}

		return out;
	}

	int base64_value(char c) {
		if (c >= 'A' && c <= 'Z') return c - 'A';
		if (c >= 'a' && c <= 'z') return c - 'a' + 26;
		if (c >= '0' && c <= '9') return c - '0' + 52;
		if (c == '+') return 62;
		if (c == '/') return 63;
		return -1;
	}

	std::vector<unsigned char> base64_decode(const std::string& value) {
		if (value.size() % 4 != 0) {
			throw std::runtime_error("Invalid base64 length.");
		}

		std::vector<unsigned char> out;
		out.reserve((value.size() / 4) * 3);
		for (std::size_t i = 0; i < value.size(); i += 4) {
			const int v0 = base64_value(value[i]);
			const int v1 = base64_value(value[i + 1]);
			const int v2 = value[i + 2] == '=' ? 0 : base64_value(value[i + 2]);
			const int v3 = value[i + 3] == '=' ? 0 : base64_value(value[i + 3]);
			if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
				throw std::runtime_error("Invalid base64 character.");
			}

			const unsigned int triple = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
			out.push_back(static_cast<unsigned char>((triple >> 16) & 0xff));
			if (value[i + 2] != '=') out.push_back(static_cast<unsigned char>((triple >> 8) & 0xff));
			if (value[i + 3] != '=') out.push_back(static_cast<unsigned char>(triple & 0xff));
		}

		return out;
	}

	std::array<std::string, 3> split_payload(const std::string& stored) {
		std::array<std::string, 3> parts;
		std::size_t start = PREFIX_SIZE;
		for (std::size_t i = 0; i < parts.size(); ++i) {
			const std::size_t end = i + 1 == parts.size() ? std::string::npos : stored.find(':', start);
			if (end == std::string::npos && i + 1 != parts.size()) {
				throw std::runtime_error("Invalid encrypted payload.");
			}
			parts[i] = stored.substr(start, end == std::string::npos ? std::string::npos : end - start);
			start = end + 1;
		}
		return parts;
	}

	std::vector<unsigned char> data_key() {
		const std::string key = env_value("BOT_DATA_KEY");
		if (key.size() < 32) {
			throw std::runtime_error("BOT_DATA_KEY must be at least 32 characters.");
		}
		return sha256(key);
	}

	std::string aes_gcm_encrypt(const std::string& plaintext) {
		const auto key = data_key();
		const auto nonce = random_bytes(NONCE_SIZE);
		std::vector<unsigned char> cipher(plaintext.size());
		std::vector<unsigned char> tag(TAG_SIZE);

		BCRYPT_ALG_HANDLE alg = nullptr;
		BCRYPT_KEY_HANDLE key_handle = nullptr;
		if (!nt_ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0))
			|| !nt_ok(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0))
			|| !nt_ok(BCryptGenerateSymmetricKey(alg, &key_handle, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0))) {
			if (key_handle) BCryptDestroyKey(key_handle);
			if (alg) BCryptCloseAlgorithmProvider(alg, 0);
			throw std::runtime_error("Could not initialize AES-GCM encryption.");
		}

		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
		BCRYPT_INIT_AUTH_MODE_INFO(info);
		info.pbNonce = const_cast<PUCHAR>(nonce.data());
		info.cbNonce = static_cast<ULONG>(nonce.size());
		info.pbTag = tag.data();
		info.cbTag = static_cast<ULONG>(tag.size());

		ULONG written = 0;
		const bool ok = nt_ok(BCryptEncrypt(
			key_handle,
			reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())),
			static_cast<ULONG>(plaintext.size()),
			&info,
			nullptr,
			0,
			cipher.data(),
			static_cast<ULONG>(cipher.size()),
			&written,
			0
		));

		BCryptDestroyKey(key_handle);
		BCryptCloseAlgorithmProvider(alg, 0);
		if (!ok || written != cipher.size()) {
			throw std::runtime_error("Could not encrypt sensitive text.");
		}

		return std::string(PREFIX)
			+ base64_encode(nonce) + ":"
			+ base64_encode(cipher) + ":"
			+ base64_encode(tag);
	}

	std::string aes_gcm_decrypt(const std::string& stored) {
		const auto parts = split_payload(stored);
		const auto key = data_key();
		const auto nonce = base64_decode(parts[0]);
		const auto cipher = base64_decode(parts[1]);
		const auto tag = base64_decode(parts[2]);
		std::vector<unsigned char> plaintext(cipher.size());

		BCRYPT_ALG_HANDLE alg = nullptr;
		BCRYPT_KEY_HANDLE key_handle = nullptr;
		if (!nt_ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0))
			|| !nt_ok(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0))
			|| !nt_ok(BCryptGenerateSymmetricKey(alg, &key_handle, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0))) {
			if (key_handle) BCryptDestroyKey(key_handle);
			if (alg) BCryptCloseAlgorithmProvider(alg, 0);
			throw std::runtime_error("Could not initialize AES-GCM decryption.");
		}

		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
		BCRYPT_INIT_AUTH_MODE_INFO(info);
		info.pbNonce = const_cast<PUCHAR>(nonce.data());
		info.cbNonce = static_cast<ULONG>(nonce.size());
		info.pbTag = const_cast<PUCHAR>(tag.data());
		info.cbTag = static_cast<ULONG>(tag.size());

		ULONG written = 0;
		const bool ok = nt_ok(BCryptDecrypt(
			key_handle,
			const_cast<PUCHAR>(cipher.data()),
			static_cast<ULONG>(cipher.size()),
			&info,
			nullptr,
			0,
			plaintext.data(),
			static_cast<ULONG>(plaintext.size()),
			&written,
			0
		));

		BCryptDestroyKey(key_handle);
		BCryptCloseAlgorithmProvider(alg, 0);
		if (!ok || written != plaintext.size()) {
			throw std::runtime_error("Could not decrypt sensitive text.");
		}

		return std::string(reinterpret_cast<const char*>(plaintext.data()), plaintext.size());
	}
}

bool sensitive_text::encryption_available() {
	return env_value("BOT_DATA_KEY").size() >= 32;
}

bool sensitive_text::is_encrypted(const std::string& value) {
	return value.rfind(PREFIX, 0) == 0;
}

std::string sensitive_text::protect(const std::string& plaintext) {
	if (plaintext.empty() || !encryption_available()) {
		return plaintext;
	}

	return aes_gcm_encrypt(plaintext);
}

std::string sensitive_text::reveal(const std::string& stored) {
	if (!is_encrypted(stored)) {
		return stored;
	}

	try {
		return aes_gcm_decrypt(stored);
	}
	catch (...) {
		return "[encrypted: unavailable]";
	}
}
