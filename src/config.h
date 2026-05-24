
#define CFG_BUF_LEN 200

struct cfg_item {
	char const *name;
	char present;
	char _data[CFG_BUF_LEN];

	char const* get();
	void set(char const *str);
	void unset();
	double getDouble();
};

extern cfg_item cfg_host;
extern cfg_item cfg_port;
extern cfg_item cfg_fov_1;
extern cfg_item cfg_fov_2;
extern cfg_item cfg_sensitivity_1;
extern cfg_item cfg_sensitivity_2;
extern cfg_item cfg_aim_1;
extern cfg_item cfg_aim_2;
extern cfg_item cfg_cam_angle_1;
extern cfg_item cfg_cam_angle_2;
extern cfg_item cfg_cam_dist_1;
extern cfg_item cfg_cam_dist_2;
extern cfg_item cfg_pred_shot_self;
extern cfg_item cfg_pred_shot_others;
extern cfg_item cfg_no_ui;

extern cfg_item* cfg_lookup(char const *name);

extern void config_init();
extern void config_destroy();
extern void config_write();
