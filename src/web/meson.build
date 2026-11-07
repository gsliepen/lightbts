htmx_path = get_option('htmx')
asset_files = []
if htmx_path != ''
	if import('fs').is_file(htmx_path)
		asset_files += htmx_path
	else
		warning('htmx not found at ' + htmx_path + ', building lbts-web without htmx')
	endif
endif

assets = custom_target('gen-web-assets',
	input: asset_files,
	output: 'assets.inl',
	capture: true,
	command: [gentemplates, 'embedded_assets', '@INPUT@'],
)

web_handler = static_library('lightbts-web',
	'handler.cpp',
	assets,
	dependencies: core,
)

web = declare_dependency(
	link_with: web_handler,
	include_directories: include_directories('.'),
	dependencies: core,
)

lbts_web = executable('lbts-web',
	'fastcgi-frontend.cpp',
	'http-frontend.cpp',
	'main.cpp',
	dependencies: [web, httplib, fastcgi],
	install: true,
)
