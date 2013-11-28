#ifndef _plat_wxwidgets__menu_tracelog__hpp__included__
#define _plat_wxwidgets__menu_tracelog__hpp__included__

#include "core/dispatch.hpp"
#include <wx/string.h>
#include <wx/wx.h>
#include <map>
#include <set>
#include <vector>

class tracelog_menu : public wxMenu
{
public:
	tracelog_menu(wxWindow* win, int wxid_low, int wxid_high);
	~tracelog_menu();
	void on_select(wxCommandEvent& e);
	void update();
	bool any_enabled();
private:
	struct dispatch_target<> corechange;
	wxWindow* pwin;
	int wxid_range_low;
	int wxid_range_high;
	std::vector<wxMenuItem*> items;
};

#endif
