project('LightBTS', 'cpp',
	version: '0.1',
	license: 'GPL-3.0-or-later',
	meson_version: '>=0.60',
	default_options: ['cpp_std=c++23', 'warning_level=3'],
	subproject_dir: 'external',
)

cpp = meson.get_compiler('cpp')

# Designated initializers that leave out members are fine.
add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')

# Dependencies
blake2  = cpp.find_library('b2')
sqlite3 = dependency('sqlite3')

# Use an installed Mimesis if possible, otherwise the submodule in external/mimesis.
mimesis = dependency('mimesis', version: '>=0.2', required: false)
if not mimesis.found()
	mimesis_project = subproject('mimesis', default_options: ['tests=disabled', 'docs=disabled'])
	mimesis = declare_dependency(
		link_with: mimesis_project.get_variable('libmimesis_internal'),
		include_directories: mimesis_project.get_variable('incdir'),
	)
endif

# Dependencies of the web server
web_option = get_option('web')
httplib = dependency('cpp-httplib', required: web_option)
fastcgi = dependency('fcgi', required: web_option)
has_mustache = cpp.has_header('kainjow/mustache.hpp', required: web_option)
build_web = httplib.found() and fastcgi.found() and has_mustache

subdir('src')
subdir('test')
subdir('doc')
