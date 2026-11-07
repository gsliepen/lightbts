shell_tests = ['basic', 'config', 'create', 'import', 'reply', 'list', 'show', 'action']

foreach name: shell_tests
	test(name, files(name + '.test'), depends: lbts, workdir: meson.project_build_root())
endforeach
