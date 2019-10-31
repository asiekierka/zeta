mergeInto(LibraryManager.library, {
	cpu_ext_log: function(s) {
		console.log(Pointer_stringify(s));
	},
	zeta_time_ms: function() {
		return vfsg_time_ms();
	},
	vfs_open: function(fn, mode) {
		return vfsg_open(fn, mode);
	},
	vfs_seek: function(h, pos, type) {
		return vfsg_seek(h, pos, type);
	},
	vfs_read: function(h, ptr, amount) {
		return vfsg_read(h, ptr, amount);
	},
	vfs_write: function(h, ptr, amount) {
		return vfsg_write(h, ptr, amount);
	},
	vfs_close: function(h) {
		return vfsg_close(h);
	},
	vfs_findfirst: function(ptr, mask, spec) {
		return vfsg_findfirst(ptr, mask, spec);
	},
	vfs_findnext: function(ptr) {
		return vfsg_findnext(ptr);
	},
	zeta_has_feature: function(id) {
		return vfsg_has_feature(id);
	},
	zeta_update_charset: function(width, height, char_ptr) {
		return zetag_update_charset(width, height, char_ptr);
	},
	zeta_update_palette: function(data_ptr) {
		return zetag_update_palette(data_ptr);
	},
	speaker_on: function(cycles, freq) {
		speakerg_on(cycles, freq);
	},
	speaker_off: function(cycles) {
		speakerg_off(cycles);
	}
});

