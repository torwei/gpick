/*
 * Copyright (c) 2009-2020, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Types.h"
#include "Map.h"
#include "Color.h"
#include <algorithm>
#include <boost/endian/conversion.hpp>
namespace dynv {
namespace xml {
bool serialize(std::ostream &stream, const Map &map, bool addRootElement = true);
}
namespace types {
static const KnownHandler knownHandlers[] = {
	{ "bool", ValueType::basicBool },
	{ "float", ValueType::basicFloat },
	{ "int32", ValueType::basicInt32 },
	{ "string", ValueType::string },
	{ "color", ValueType::color },
	{ "dynv", ValueType::map },
};
const size_t knownHandlerCount = sizeof(knownHandlers) / sizeof(KnownHandler);
ValueType stringToType(const char *value) {
	if (value == nullptr || value[0] == 0)
		return ValueType::unknown;
	for (size_t i = 0; i < knownHandlerCount; i++) {
		if (knownHandlers[i].name == value)
			return knownHandlers[i].type;
	}
	return ValueType::unknown;
}
ValueType stringToType(const std::string &value) {
	if (value.empty())
		return ValueType::unknown;
	for (size_t i = 0; i < knownHandlerCount; i++) {
		if (knownHandlers[i].name == value)
			return knownHandlers[i].type;
	}
	return ValueType::unknown;
}
template<> const KnownHandler &typeHandler<bool>() { return knownHandlers[0]; }
template<> const KnownHandler &typeHandler<float>() { return knownHandlers[1]; }
template<> const KnownHandler &typeHandler<int32_t>() { return knownHandlers[2]; }
template<> const KnownHandler &typeHandler<std::string>() { return knownHandlers[3]; }
template<> const KnownHandler &typeHandler<Color>() { return knownHandlers[4]; }
template<> const KnownHandler &typeHandler<Ref>() { return knownHandlers[5]; }
namespace xml {
static std::string escapeXmlString(const std::string &value) {
	std::string result;
	result.reserve(value.length() + 64);
	for (auto ch: value) {
		switch (ch) {
		case '&':
			result += "&amp;";
			break;
		case '<':
			result += "&lt;";
			break;
		case '>':
			result += "&gt;";
			break;
		default:
			result += ch;
		}
	}
	return result;
}
template<> bool write(std::ostream &stream, bool value) {
	stream << (value ? "true" : "false");
	return stream.good();
}
template<> bool write(std::ostream &stream, float value) {
	stream << value;
	return stream.good();
}
template<> bool write(std::ostream &stream, int32_t value) {
	stream << value;
	return stream.good();
}
template<> bool write(std::ostream &stream, const std::string &value) {
	stream << escapeXmlString(value);
	return stream.good();
}
template<> bool write(std::ostream &stream, const Color &value) {
	stream << value.ma[0] << " " << value.ma[1] << " " << value.ma[2] << " " << value.ma[3];
	return stream.good();
}
template<> bool write(std::ostream &stream, const Ref &value) {
	if (value)
		dynv::xml::serialize(stream, *value, false);
	return stream.good();
}
}
namespace binary {
template<> bool write(std::ostream &stream, uint8_t value) {
	stream.write(reinterpret_cast<const char *>(&value), 1);
	return stream.good();
}
template<> bool write(std::ostream &stream, bool value) {
	return write(stream, static_cast<uint8_t>(value ? 1 : 0));
}
template<> bool write(std::ostream &stream, float value) {
	union {
		float f;
		uint32_t i;
	} convert = { value };
	auto intValue = boost::endian::native_to_little(convert.i);
	stream.write(reinterpret_cast<const char *>(&intValue), 4);
	return stream.good();
}
template<> bool write(std::ostream &stream, int32_t value) {
	value = boost::endian::native_to_little(value);
	stream.write(reinterpret_cast<const char *>(&value), 4);
	return stream.good();
}
template<> bool write(std::ostream &stream, uint32_t value) {
	value = boost::endian::native_to_little(value);
	stream.write(reinterpret_cast<const char *>(&value), 4);
	return stream.good();
}
template<> bool write(std::ostream &stream, const std::string &value) {
	if (!write(stream, static_cast<uint32_t>(value.length())))
		return false;
	stream.write(value.c_str(), value.length());
	return stream.good();
}
template<> bool write(std::ostream &stream, const Color &value) {
	if (!write(stream, static_cast<uint32_t>(sizeof(value.ma))))
		return false;
	if (!write(stream, static_cast<float>(value.ma[0])))
		return false;
	if (!write(stream, static_cast<float>(value.ma[1])))
		return false;
	if (!write(stream, static_cast<float>(value.ma[2])))
		return false;
	if (!write(stream, static_cast<float>(value.ma[3])))
		return false;
	return true;
}
template<> uint8_t read(std::istream &stream) {
	uint8_t value;
	stream.read(reinterpret_cast<char *>(&value), 1);
	return value;
}
template<> uint32_t read(std::istream &stream) {
	uint32_t value;
	stream.read(reinterpret_cast<char *>(&value), 4);
	return boost::endian::little_to_native(value);
}
template<> int32_t read(std::istream &stream) {
	int32_t value;
	stream.read(reinterpret_cast<char *>(&value), 4);
	return boost::endian::little_to_native(value);
}
template<> std::string read(std::istream &stream) {
	uint32_t length = read<uint32_t>(stream);
	if (!stream.good())
		return std::string();
	std::string result;
	result.resize(length);
	stream.read(reinterpret_cast<char *>(&result.front()), length);
	return result;
}
template<> float read(std::istream &stream) {
	float value;
	static_assert(sizeof(value) == 4);
	stream.read(reinterpret_cast<char *>(&value), sizeof(value));
	return boost::endian::little_to_native(value);
}
template<> Color read(std::istream &stream) {
	float values[4];
	static_assert(sizeof(values) == 16);
	auto storeLength = read<uint32_t>(stream);
	auto length = std::min<uint32_t>(storeLength, sizeof(values));
	if (length > 0)
		stream.read(reinterpret_cast<char *>(&values), length);
	if (storeLength - length > 0)
		stream.seekg(storeLength - length, std::ios::cur);
	Color result;
	for (int i = 0; i < 4; i++) {
		result.ma[i] = boost::endian::little_to_native(values[i]);
	}
	return result;
}
}
}
}
