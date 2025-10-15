//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "iominimap.h"

#include "tile.h"
#include "filehandle.h"
#include "editor.h"
#include "gui.h"

#include <wx/image.h>
#include <zlib.h>

void MinimapBlock::updateTile(int x, int y, const MinimapTile &tile) {
	m_tiles[getTileIndex(x, y)] = tile;
}

IOMinimap::IOMinimap(Editor* editor, MinimapExportFormat format, MinimapExportMode mode, bool updateLoadbar, int imageSize /* = 1024 */) :
	m_editor(editor),
	m_format(format),
	m_mode(mode),
	m_updateLoadbar(updateLoadbar),
	m_imageSize(imageSize) {
}

bool IOMinimap::saveMinimap(const std::string &directory, const std::string &name, int floor) {
	if (m_mode == MinimapExportMode::AllFloors || m_mode == MinimapExportMode::SelectedArea) {
		floor = -1;
	} else if (m_mode == MinimapExportMode::GroundFloor) {
		floor = rme::MapGroundLayer;
	} else if (m_mode == MinimapExportMode::SpecificFloor) {
		if (floor < rme::MapMinLayer || floor > rme::MapMaxLayer) {
			floor = rme::MapGroundLayer;
		}
	}

	m_floor = floor;

	if (m_format == MinimapExportFormat::Otmm) {
		return saveOtmm(wxFileName(directory, name + ".otmm"));
	}
	return saveImage(directory, name);
}

bool IOMinimap::saveOtmm(const wxFileName &file) {
	try {
		FileWriteHandle writer(file.GetFullPath().ToStdString());
		if (!writer.isOk()) {
			// error("Unable to open file %s for save minimap", file);
			return false;
		}

		// TODO: compression flag with zlib
		uint32_t flags = 0;

		// header
		writer.addU32(OTMM_SIGNATURE);
		writer.addU16(0); // data start, will be overwritten later
		writer.addU16(OTMM_VERSION);
		writer.addU32(flags);

		// version 1 header
		writer.addString("OTMM 1.0"); // description

		// go back and rewrite where the map data starts
		uint32_t start = writer.tell();
		writer.seek(4);
		writer.addU16(start);
		writer.seek(start);

		unsigned long blockSize = MMBLOCK_SIZE * MMBLOCK_SIZE * sizeof(MinimapTile);
		std::vector<uint8_t> buffer(compressBound(blockSize));
		constexpr int COMPRESS_LEVEL = 3;

		readBlocks();

		for (uint8_t z = 0; z <= rme::MapMaxLayer; ++z) {
			for (auto &it : m_blocks[z]) {
				int index = it.first;
				auto &block = it.second;

				// write index pos
				uint16_t x = static_cast<uint16_t>((index % (65536 / MMBLOCK_SIZE)) * MMBLOCK_SIZE);
				uint16_t y = static_cast<uint16_t>((index / (65536 / MMBLOCK_SIZE)) * MMBLOCK_SIZE);
				writer.addU16(x);
				writer.addU16(y);
				writer.addU8(z);

				unsigned long len = blockSize;
				int ret = compress2(buffer.data(), &len, (uint8_t*)&block.getTiles(), blockSize, COMPRESS_LEVEL);
				assert(ret == Z_OK);
				writer.addU16(len);
				writer.addRAW(buffer.data(), len);
			}
			m_blocks[z].clear();
		}

		// end of file is an invalid pos
		writer.addU16(65535);
		writer.addU16(65535);
		writer.addU8(255);

		writer.flush();
		writer.close();
	} catch (std::exception &e) {
		m_error = wxString::Format("failed to save OTMM minimap: %s", e.what());
		return false;
	}

	return true;
}

bool IOMinimap::saveImage(const std::string &directory, const std::string &name) {
	try {
		switch (m_mode) {
			case MinimapExportMode::AllFloors:
			case MinimapExportMode::GroundFloor:
			case MinimapExportMode::SpecificFloor: {
				exportMinimap(directory);
				break;
			}
			case MinimapExportMode::SelectedArea: {
				exportSelection(directory, name);
				break;
			}
		}
	} catch (std::bad_alloc &) {
		m_error = "There is not enough memory available to complete the operation.";
	}

	return true;
}

bool IOMinimap::exportMinimap(const std::string &directory) {
	auto &map = m_editor->getMap();
	if (map.size() == 0) {
		return true;
	}

	int min_z = m_floor == -1 ? 0 : m_floor;
	int max_z = m_floor == -1 ? rme::MapMaxLayer : m_floor;
	int max_x = 0, max_y = 0;
	for (auto it = map.begin(); it != map.end(); ++it) {
		auto tile = (*it)->get();
		if (!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}
		const auto &position = tile->getPosition();
		if (position.x > max_x) {
			max_x = position.x;
		}
		if (position.y > max_y) {
			max_y = position.y;
		}
	}

	int last_x = ((max_x / m_imageSize) + 1) * m_imageSize - 1;
	int last_y = ((max_y / m_imageSize) + 1) * m_imageSize - 1;

	int pixels_size = m_imageSize * m_imageSize * rme::PixelFormatRGB;
	uint8_t* pixels = new uint8_t[pixels_size];
	auto image = new wxImage(m_imageSize, m_imageSize, pixels, true);

	int processedTiles = 0;
	int lastShownProgress = -1;

	for (size_t z = min_z; z <= max_z; z++) {
		for (int h = 0; h <= last_y; h += m_imageSize) {
			for (int w = 0; w <= last_x; w += m_imageSize) {
				memset(pixels, 0, pixels_size);
				bool empty = true;

				int index = 0;
				for (int y = 0; y < m_imageSize; y++) {
					for (int x = 0; x < m_imageSize; x++) {
						int tile_x = w + x;
						int tile_y = h + y;
						if (tile_x > max_x || tile_y > max_y) {
							index += rme::PixelFormatRGB;
							continue;
						}
						auto tile = map.getTile(tile_x, tile_y, z);
						if (!tile || (!tile->ground && tile->items.empty())) {
							index += rme::PixelFormatRGB;
							continue;
						}

						processedTiles++;
						int progress = static_cast<int>((static_cast<double>(processedTiles) / map.size()) * 100);
						if (progress > lastShownProgress) {
							if (m_updateLoadbar) {
								g_gui.SetLoadDone(progress);
							}
							lastShownProgress = progress;
						}

						uint8_t color = tile->getMiniMapColor();
						pixels[index] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51);
						pixels[index + 1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51);
						pixels[index + 2] = (uint8_t)(color % 6 * 51);
						index += rme::PixelFormatRGB;
						empty = false;
					}
				}

				if (!empty) {
					image->SetData(pixels, true);
					wxString extension = m_format == MinimapExportFormat::Png ? "png" : "bmp";
					wxBitmapType type = m_format == MinimapExportFormat::Png ? wxBITMAP_TYPE_PNG : wxBITMAP_TYPE_BMP;
					wxString extension_wx = wxString::FromAscii(extension.mb_str());
					wxFileName file = wxString::Format("Minimap_Color_%d_%d_%d.%s", w, h, (int)z, extension_wx);
					file.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_CASE | wxPATH_NORM_ABSOLUTE, directory);
					image->SaveFile(file.GetFullPath(), type);
				}
			}
		}
	}

	g_gui.DestroyLoadBar();
	image->Destroy();
	delete[] pixels;
	return true;
}

bool IOMinimap::exportSelection(const std::string &directory, const std::string &name) {
	int min_x = rme::MapMaxWidth + 1;
	int min_y = rme::MapMaxHeight + 1;
	int min_z = rme::MapMaxLayer + 1;
	int max_x = 0, max_y = 0, max_z = 0;

	const auto &selection = m_editor->getSelection();
	const auto &tiles = selection.getTiles();

	for (auto tile : tiles) {
		if (!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}

		const auto &position = tile->getPosition();
		if (position.x < min_x) {
			min_x = position.x;
		}
		if (position.x > max_x) {
			max_x = position.x;
		}

		if (position.y < min_y) {
			min_y = position.y;
		}
		if (position.y > max_y) {
			max_y = position.y;
		}

		if (position.z < min_z) {
			min_z = position.z;
		}
		if (position.z > max_z) {
			max_z = position.z;
		}
	}

	int numtiles = (max_x - min_x) * (max_y - min_y);
	if (numtiles == 0) {
		return false;
	}

	int image_width = max_x - min_x + 1;
	int image_height = max_y - min_y + 1;
	if (image_width > 2048 || image_height > 2048) {
		g_gui.PopupDialog("Error", "Minimap size greater than 2048px.", wxOK);
		return false;
	}

	int pixels_size = image_width * image_height * rme::PixelFormatRGB;
	uint8_t* pixels = new uint8_t[pixels_size];
	auto image = new wxImage(image_width, image_height, pixels, true);

	int tiles_iterated = 0;
	for (int z = min_z; z <= max_z; z++) {
		bool empty = true;
		memset(pixels, 0, pixels_size);
		for (auto tile : tiles) {
			if (tile->getZ() != z) {
				continue;
			}

			if (m_updateLoadbar) {
				tiles_iterated++;
				if (tiles_iterated % 8192 == 0) {
					g_gui.SetLoadDone(int(tiles_iterated / double(tiles.size()) * 90.0));
				}
			}

			if (!tile->ground && tile->items.empty()) {
				continue;
			}

			uint8_t color = tile->getMiniMapColor();
			uint32_t index = ((tile->getY() - min_y) * image_width + (tile->getX() - min_x)) * 3;
			pixels[index] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51); // red
			pixels[index + 1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51); // green
			pixels[index + 2] = (uint8_t)(color % 6 * 51); // blue
			empty = false;
		}

		if (!empty) {
			image->SetData(pixels, true);
			wxString extension = m_format == MinimapExportFormat::Png ? "png" : "bmp";
			wxBitmapType type = m_format == MinimapExportFormat::Png ? wxBITMAP_TYPE_PNG : wxBITMAP_TYPE_BMP;
			wxFileName file = wxString::Format("%s-%d.%s", name, z, extension);
			file.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_TILDE | wxPATH_NORM_CASE, directory);
			image->SaveFile(file.GetFullPath(), type);
		}
	}

	image->Destroy();
	delete[] pixels;
	return true;
}

void IOMinimap::readBlocks() {
	if (m_mode == MinimapExportMode::SelectedArea && !m_editor->hasSelection()) {
		return;
	}

	auto &map = m_editor->getMap();

	int tiles_iterated = 0;
	for (auto it = map.begin(); it != map.end(); ++it) {
		auto tile = (*it)->get();

		if (m_updateLoadbar) {
			++tiles_iterated;
			if (tiles_iterated % 8192 == 0) {
				g_gui.SetLoadDone(int(tiles_iterated / double(map.size()) * 90.0));
			}
		}

		if (!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}

		const auto &position = tile->getPosition();

		if (m_mode == MinimapExportMode::SelectedArea) {
			if (!tile->isSelected()) {
				continue;
			}
		} else if (m_floor != -1 && position.z != m_floor) {
			continue;
		}

		MinimapTile minimapTile;
		minimapTile.color = tile->getMiniMapColor();
		minimapTile.flags |= MinimapTileWasSeen;
		if (tile->isBlocking()) {
			minimapTile.flags |= MinimapTileNotWalkable;
		}
		// if (!tile->isPathable()) {
		// minimapTile.flags |= MinimapTileNotPathable;
		//}
		minimapTile.speed = std::min<int>((int)std::ceil(tile->getGroundSpeed() / 10.f), 0xFF);

		auto &blocks = m_blocks[position.z];
		uint32_t index = getBlockIndex(position);
		if (blocks.find(index) == blocks.end()) {
			blocks.insert({ index, MinimapBlock() });
		}

		auto &block = blocks.at(index);
		int offset_x = position.x - (position.x % MMBLOCK_SIZE);
		int offset_y = position.y - (position.y % MMBLOCK_SIZE);
		block.updateTile(position.x - offset_x, position.y - offset_y, minimapTile);
	}
}
