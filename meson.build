project('LightBTS', 'cpp',
	version: '0.1',
	license: 'GPL3+',
)

cpp = meson.get_compiler('cpp')

# Dependencies
blake2  = cpp.find_library('b2')
boost   = dependency('boost', modules: ['filesystem', 'system'])
fmtlib  = dependency('fmt', required: false)
if not fmtlib.found()
	fmtlib = cpp.find_library('fmt')
endif
mimesis = dependency('mimesis')
sqlite3 = dependency('sqlite3')

subdir('src')
subdir('test')
