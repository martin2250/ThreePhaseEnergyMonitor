#!/usr/bin/python
Import("env")

def prebuild(source, target, env):
	print("test")

env.AddPreAction("build", prebuild)
