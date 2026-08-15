module;
#include <iostream>
export module FuzeHttp.PermissionSetting;

export namespace FuzeHttp {
enum struct THREE_STATE_SETTING { DENY = 0, INHERIT = 1, ALLOW = 2 };

// A seperate PermissionSetting class is used for futureproofing; 
// in Permissions 2 custom constraints will be added.
class PermissionSetting {
public:
	PermissionSetting(int id, THREE_STATE_SETTING setting)
			: id(id),
			setting(setting) {
	}
	bool getBool(bool inherited_permission) const {
		if (this->setting == THREE_STATE_SETTING::INHERIT)
			return inherited_permission;
		else {
			// TODO only return non-inherited setting when constraints (if any) return true
			return this->setting == THREE_STATE_SETTING::ALLOW;
		}
	}
	THREE_STATE_SETTING get() const { return this->setting; }
	void set(THREE_STATE_SETTING setting) { this->setting = setting; }
	int getId() const { return this->id; }
private:
	int id;
	// int permission_collection_id;
	// PERMISSION permission;
	THREE_STATE_SETTING setting;
};
} // namespace FuzeHttp
