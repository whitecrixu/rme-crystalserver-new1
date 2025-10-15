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

#ifndef RME_PALETTE_BRUSHLIST_
#define RME_PALETTE_BRUSHLIST_

#include "main.h"
#include "palette_common.h"

enum BrushListType {
	BRUSHLIST_LARGE_ICONS,
	BRUSHLIST_SMALL_ICONS,
	BRUSHLIST_LISTBOX,
	BRUSHLIST_TEXT_LISTBOX,
};

static const std::unordered_map<wxString, BrushListType> listTypeMap = {
	{ "small icons", BRUSHLIST_SMALL_ICONS },
	{ "large icons", BRUSHLIST_LARGE_ICONS },
	{ "listbox", BRUSHLIST_LISTBOX },
	{ "textlistbox", BRUSHLIST_TEXT_LISTBOX },
};

class BrushBoxInterface {
public:
	explicit BrushBoxInterface(const TilesetCategory* tileset) noexcept :
		tileset(tileset) {
		ASSERT(tileset);
	}
	virtual ~BrushBoxInterface() = default;

	virtual wxWindow* GetSelfWindow() = 0;

	// Select the first brush
	virtual void SelectFirstBrush() = 0;
	// Returns the currently selected brush (First brush if panel is not loaded)
	virtual Brush* GetSelectedBrush() const = 0;
	// Select the brush in the parameter, this only changes the look of the panel
	virtual bool SelectPaginatedBrush(const Brush* brush, BrushPalettePanel* brushPalettePanel) = 0;
	virtual bool SelectBrush(const Brush* brush) = 0;

	virtual bool NextPage() = 0;
	virtual bool SetPage(int page) = 0;
	virtual bool PreviousPage() = 0;

	virtual int GetCurrentPage() {
		return currentPage;
	}

	virtual int GetTotalPages() {
		return totalPages;
	}

protected:
	const TilesetCategory* const tileset;
	bool loaded = false;
	int currentPage = 1;
	int totalPages = 1;
};

class BrushListBox : public wxVListBox, public BrushBoxInterface {
public:
	BrushListBox(wxWindow* parent, const TilesetCategory* tileset);
	~BrushListBox() final = default;

	wxWindow* GetSelfWindow() {
		return this;
	}

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (First brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectPaginatedBrush(const Brush* whatBrush, BrushPalettePanel* brushPalettePanel) noexcept override;
	bool SelectBrush(const Brush* whatBrush) override;

	bool NextPage() override {
		return false;
	};

	bool SetPage(int page) override {
		return false;
	};

	bool PreviousPage() override {
		return false;
	};

	// Event handlers
	void OnDrawItem(wxDC &dc, const wxRect &rect, size_t index) const override;
	wxCoord OnMeasureItem(size_t index) const override;

	void OnKey(wxKeyEvent &event);

	DECLARE_EVENT_TABLE();
};

class BrushIconBox : public wxScrolledWindow, public BrushBoxInterface {
public:
	BrushIconBox(wxWindow* parent, const TilesetCategory* tileset, RenderSize renderSize);
	~BrushIconBox() = default;

	wxWindow* GetSelfWindow() {
		return this;
	}

	// Scrolls the window to the position of the named brush button
	void EnsureVisible(const BrushButton* brushButto);

	bool LoadContentByPage(int page = 1);

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (First brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectPaginatedBrush(const Brush* whatBrush, BrushPalettePanel* brushPalettePanel) override;
	bool SelectBrush(const Brush* whatBrush) override;

	bool NextPage() override;
	bool SetPage(int page) override;
	bool PreviousPage() override;

	// Event handling...
	void OnClickBrushButton(wxCommandEvent &event);

private:
	// Used internally to select a button.
	void Select(BrushButton* brushButton);
	// Used internally to deselect a button before selecting a new one.
	void Deselect();

	int width = 0;
	int height = 0;

	BrushButton* selectedButton = nullptr;
	std::vector<BrushButton*> brushButtons;
	RenderSize iconSize;

	wxBoxSizer* stacksizer = nullptr;
	std::vector<const wxBoxSizer*> rowsizers;

	DECLARE_EVENT_TABLE();
};

// A panel capapable of displaying a collection of brushes
// Brushes can be arranged in either list or icon fashion
// Contents are *not* created when the panel is created,
// but on the first call to LoadContents(), this is to
// allow procedural loading (faster)

class BrushPanel : public wxPanel {
public:
	BrushPanel(wxWindow* parent, const TilesetCategory* tileset);
	~BrushPanel() final = default;

	// Interface
	// Flushes this panel and consequent views will feature reloaded data
	void InvalidateContents();
	// Loads the content (This must be called before the panel is displayed, else it will appear empty
	void LoadContents();

	BrushListType GetListType() const;

	// Sets the display type (list or icons)
	void SetListType(BrushListType newListType);
	void SetListType(const wxString &newListType);
	// Assigns a tileset to this list
	void AssignTileset(const TilesetCategory* newTileset);

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (First brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatBrush);

	// Called when the window is about to be displayed
	void OnSwitchIn();
	// Called when this page is hidden
	void OnSwitchOut();

	// wxWidgets event handlers
	void OnClickListBoxRow(wxCommandEvent &event);

	[[nodiscard]] BrushBoxInterface* GetBrushBox() const;

protected:
	const TilesetCategory* tileset;
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	BrushBoxInterface* brushbox;
	bool loaded = false;
	BrushListType listType = BRUSHLIST_LISTBOX;

	DECLARE_EVENT_TABLE();
};

class BrushPalettePanel : public PalettePanel {
public:
	BrushPalettePanel(wxWindow* parent, const TilesetContainer &tilesets, TilesetCategoryType category, wxWindowID id = wxID_ANY);
	~BrushPalettePanel();

	void RemovePagination();
	void AddPagination();
	void AddTilesetEditor();

	// Interface
	// Flushes this panel and consequent views will feature reloaded data
	void InvalidateContents();
	// Loads the currently displayed page
	void LoadCurrentContents();
	// Loads all content in this panel
	void LoadAllContents();

	PaletteType GetType() const;
	BrushListType GetListType() const;

	// Sets the display type (list or icons)
	void SetListType(BrushListType newListType);
	void SetListType(const wxString &newListType);

	// Select the first brush
	void SelectFirstBrush();
	// Returns the currently selected brush (first brush if panel is not loaded)
	Brush* GetSelectedBrush() const;
	// Select the brush in the parameter, this only changes the look of the panel
	bool SelectBrush(const Brush* whatBrush) override;

	// Called when this page is displayed
	void OnSwitchIn();

	// Event handler for child window
	void OnSwitchingPage(wxChoicebookEvent &event);
	void OnPageChanged(wxChoicebookEvent &event);
	void OnClickAddTileset(wxCommandEvent &WXUNUSED(event));
	void OnClickAddItemToTileset(wxCommandEvent &WXUNUSED(event));

	void OnSetFocus(wxFocusEvent &event);
	void OnKillFocus(wxFocusEvent &event);

	void OnPageUpdate(BrushBoxInterface* brushbox, int page);
	void OnSetPage(wxCommandEvent &WXUNUSED(event));
	void OnNextPage(wxCommandEvent &WXUNUSED(event));
	void OnPreviousPage(wxCommandEvent &WXUNUSED(event));

	void EnableNextPage(bool enable = true);
	void EnablePreviousPage(bool enable = true);
	void SetPageInfo(const wxString &text);
	void SetCurrentPage(const wxString &text);

protected:
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	wxSizer* pageInfoSizer = newd wxFlexGridSizer(7, 1, 1);
	PaletteType paletteType;
	wxChoicebook* choicebook = nullptr;
	wxButton* nextPageButton = nullptr;
	wxButton* previousPageButton = nullptr;
	wxTextCtrl* currentPageCtrl = nullptr;
	wxStaticText* pageInfo = nullptr;
	BrushSizePanel* sizePanel = nullptr;
	std::map<wxWindow*, Brush*> rememberedBrushes;

	DECLARE_EVENT_TABLE();
};

#endif
