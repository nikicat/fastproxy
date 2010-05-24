#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006-2010 (ita)

import Options

# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='fastproxy'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def set_options(opt):
	opt.tool_options('compiler_cxx')
	opt.add_option('--build_kind', action='store', default='debug,release', help='build the selected variants')

def configure(conf):

	# conf.env.CXX = Options.options.meow
	# to configure using another compiler, use a command-line like this
	# CXX=g++-3.0 ./waf.py configure will use g++-3.0 instead of 'g++'
	conf.check_tool('compiler_cxx')

	conf.sub_config('src')

	# here is how to override flags
	#conf.env.CXXFLAGS_MYPROG  = ['-O3']
	#if not Utils.is_win32: conf.env.LIB_MYPROG = ['m']
	#conf.env.SOME_INSTALL_DIR = '/tmp/ahoy/lib/'

	# create a debug and release builds (variants)
	dbg = conf.env.copy()
	rel = conf.env.copy()

	dbg.set_variant('debug')
	conf.set_env_name('debug', dbg)
	conf.setenv('debug')
	conf.env.CXXFLAGS = ['-D_REENTRANT', '-DDBG_ENABLED', '-Wall', '-O0', '-ggdb3', '-ftemplate-depth-128']

	rel.set_variant('release')
	conf.set_env_name('release', rel)
	conf.setenv('release')
	conf.env.CXXFLAGS = ['-O2']


def build(bld):

	bld.add_subdirs('src')

	# enable the debug or the release variant, depending on the one wanted
	for obj in bld.all_task_gen[:]:

		# task generator instances (bld.new_task_gen...) should be created in the same order
		# to avoid unnecessary rebuilds (remember that cloning task generators is cheap, but copying task instances are expensive)
		debug_obj = obj.clone('debug')
		release_obj = obj.clone('release')

		# stupid reminder: do not make clones for the same variant (default -> default)

		# disable the original task generator for the default variant (do not use it)
		obj.posted = 1

		# disable the unwanted variant(s)
		kind = Options.options.build_kind
		if kind.find('debug') < 0:
			debug_obj.posted = 1
		if kind.find('release') < 0:
			release_obj.posted = 1
