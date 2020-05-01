/*
 * Copyright (c) 2009-2016, Albertas Vyšniauskas
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

#include "FileFormat.h"
#include "ColorObject.h"
#include "ColorList.h"
#include "dynv/Map.h"
#include "common/Scoped.h"
#include "version/Version.h"
#include <string.h>
#include <fstream>
#include <list>
#include <algorithm>
#include <boost/endian/conversion.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define CHUNK_TYPE_VERSION "GPA version"
#define CHUNK_TYPE_HANDLER_MAP "handler_map"
#define CHUNK_TYPE_COLOR_LIST "color_list"
#define CHUNK_TYPE_COLOR_POSITIONS "color_positions"
#define CHUNK_TYPE_COLOR_ACTIONS "color_actions"
const uint32_t Version = static_cast<uint32_t>(1 * 0x10000 + 0);
struct ChunkHeader {
	void prepareWrite(const std::string &type, uint64_t size) {
		size_t length = type.length();
		if (length >= sizeof(m_type))
			length = sizeof(m_type) - 1;
		memcpy(m_type, type.c_str(), length);
		memset(m_type + length, 0, sizeof(m_type) - length);
		m_size = boost::endian::native_to_little<uint64_t>(size);
	}
	void prepareRead() {
		m_size = boost::endian::little_to_native<uint64_t>(m_size);
	}
	bool valid() const {
		if (m_type[sizeof(m_type) - 1] != 0)
			return false;
		return true;
	}
	uint64_t size() const {
		return m_size;
	}
	bool is(const std::string &type) const {
		return m_type == type;
	}
	bool startsWith(const std::string &type) const {
		return boost::starts_with(m_type, type);
	}
private:
	char m_type[16];
	uint64_t m_size;
};
static bool colorObjectPositionSort(ColorObject* x, ColorObject* y) {
	return x->getPosition() < y->getPosition();
}
static bool read(std::istream &stream, ChunkHeader &header) {
	stream.read(reinterpret_cast<char *>(&header), sizeof(header));
	header.prepareRead();
	return stream.good();
}
static bool read(std::istream &stream, uint32_t &value) {
	stream.read(reinterpret_cast<char *>(&value), sizeof(value));
	value = boost::endian::little_to_native<uint32_t>(value);
	return stream.good();
}
static bool read(std::istream &stream, std::string &value) {
	uint32_t length;
	if (!read(stream, length))
		return false;
	value.resize(length);
	stream.read(reinterpret_cast<char *>(&value.front()), length);
	return stream.good();
}
int palette_file_load(const char* filename, ColorList* colorList) {
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open())
		return -1;
	ChunkHeader header;
	if (!read(file, header) || !header.valid() || !header.startsWith(CHUNK_TYPE_VERSION))
		return -1;
	if (header.size() < 4)
		return -1;
	uint32_t version;
	if (!read(file, version))
		return -1;
	if (version != Version)
		return -1;
	file.seekg(header.size() - 4, std::ios::cur);
	if (!file.good())
		return -1;
	if (!read(file, header) || !header.valid()) {
		return file.eof() ? 0 : -1;
	}
	std::vector<ColorObject *> colorObjects;
	auto releaseColorObjects = common::makeScoped(std::function<void()>([&colorObjects]() {
		for (auto colorObject: colorObjects)
			if (colorObject)
				colorObject->release();
	}));
	std::vector<uint32_t> positions;
	std::unordered_map<uint8_t, dynv::types::ValueType> typeMap;
	std::vector<std::string> handlers;
	bool hasPositions = false;
	for (;;) {
		if (header.is(CHUNK_TYPE_HANDLER_MAP)) {
			uint32_t handlerCount;
			if (!read(file, handlerCount))
				return -1;
			if (handlerCount > 255)
				return -1;
			for (size_t i = 0; i < handlerCount; i++) {
				std::string typeName;
				if (!read(file, typeName))
					return -1;
				typeMap[static_cast<uint8_t>(handlers.size())] = dynv::types::stringToType(typeName);
				handlers.push_back(typeName);
			}
		} else if (header.is(CHUNK_TYPE_COLOR_LIST)) {
			std::streamoff end = file.tellg() + static_cast<std::streamoff>(header.size());
			while (file.tellg() < end) {
				dynv::Map options;
				if (!options.deserialize(file, typeMap))
					return -1;
				auto color = options.getColor("color", Color());
				auto name = options.getString("name", "");
				colorObjects.push_back(new ColorObject(name, color));
			}
		} else if (header.is(CHUNK_TYPE_COLOR_POSITIONS)) {
			hasPositions = true;
			positions.resize(header.size() / sizeof(uint32_t));
			file.read(reinterpret_cast<char *>(&positions.front()), positions.size() * sizeof(uint32_t));
			if (!file.good())
				return -1;
			for (auto &position: positions) {
				position = boost::endian::little_to_native<uint32_t>(position);
			}
		} else {
			file.seekg(header.size(), std::ios::cur);
			if (file.eof())
				break;
			if (!file.good())
				return -1;
		}
		if (!read(file, header) || !header.valid()) {
			if (file.eof())
				break;
			else
				return -1;
		}
	}
	if (hasPositions) {
		for (size_t i = 0, end = std::min(colorObjects.size(), positions.size()); i < end; i++) {
			colorObjects[i]->setPosition(positions[i]);
		}
		std::sort(colorObjects.begin(), colorObjects.end(), colorObjectPositionSort);
	}
	for (auto colorObject: colorObjects) {
		bool visible = hasPositions ? colorObject->getPosition() != ~(size_t)0 : true;
		colorObject->setVisible(visible);
		color_list_add_color_object(colorList, colorObject, visible);
	}
	file.close();
	return file.good();
}
static bool write(std::ostream &stream, uint32_t value) {
	auto data = boost::endian::native_to_little<uint32_t>(value);
	static_assert(sizeof(data) == 4);
	stream.write(reinterpret_cast<const char *>(&data), sizeof(uint32_t));
	return stream.good();
}
static bool write(std::ostream &stream, const ChunkHeader &header) {
	static_assert(sizeof(header) == 24);
	stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
	return stream.good();
}
static bool write(std::ostream &stream, const std::string &value) {
	if (!write(stream, static_cast<uint32_t>(value.length())))
		return false;
	stream.write(reinterpret_cast<const char *>(&value.front()), value.length());
	return stream.good();
}
int palette_file_save(const char* filename, ColorList* colorList) {
	if (!filename || !colorList)
		return -1;
	std::ofstream file(filename, std::ios::binary);
	if (!file.is_open())
		return -1;
	ChunkHeader header;
	header.prepareWrite(std::string(CHUNK_TYPE_VERSION) + " " + gpick_build_version, 4);
	if (!write(file, header))
		return -1;
	if (!write(file, Version)) // file format version
		return -1;
	auto handlerMapPosition = file.tellp();
	if (!write(file, header)) // write temporary chunk header
		return -1;
	if (!write(file, static_cast<uint32_t>(2))) // handler count for colors
		return -1;
	std::unordered_map<dynv::types::ValueType, uint8_t> typeMap;
	typeMap[dynv::types::typeHandler<Color>().type] = 0;
	if (!write(file, dynv::types::typeHandler<Color>().name))
		return -1;
	typeMap[dynv::types::typeHandler<std::string>().type] = 1;
	if (!write(file, dynv::types::typeHandler<std::string>().name))
		return -1;
	auto endPosition = file.tellp();
	file.seekp(handlerMapPosition);
	header.prepareWrite(CHUNK_TYPE_HANDLER_MAP, endPosition - handlerMapPosition - sizeof(ChunkHeader));
	if (!write(file, header)) // write type handler chunk header
		return -1;
	file.seekp(endPosition);
	auto colorListPosition = file.tellp();
	if (!write(file, header)) // write temporary chunk header
		return -1;
	dynv::Map options;
	for (auto colorObject: colorList->colors) {
		options.set("name", colorObject->getName());
		options.set("color", colorObject->getColor());
		if (!options.serialize(file, typeMap))
			return -1;
	}
	endPosition = file.tellp();
	file.seekp(colorListPosition);
	header.prepareWrite(CHUNK_TYPE_COLOR_LIST, endPosition - colorListPosition - sizeof(ChunkHeader));
	if (!write(file, header)) // write color list chunk header
		return -1;
	file.seekp(endPosition);
	color_list_get_positions(colorList);
	std::vector<uint32_t> positions(colorList->colors.size());
	size_t i = 0;
	for (auto color: colorList->colors) {
		positions[i++] = boost::endian::native_to_little<uint32_t>(color->getPosition());
	}
	header.prepareWrite(CHUNK_TYPE_COLOR_POSITIONS, positions.size() * sizeof(uint32_t));
	if (!write(file, header)) // write positions chunk header
		return -1;
	file.write(reinterpret_cast<const char *>(&positions.front()), positions.size() * sizeof(uint32_t));
	if (!file.good())
		return -1;
	file.close();
	return file.good();
}
