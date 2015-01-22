from distutils.core import setup, Extension
from glob import glob
from os.path import join

version = "0.4.1"

bindir = "bin"
docdir = join ("share", "doc", "plife-" + version)

setup (
	name = "plife-python",
	version = version,
	description = "Pattern construction tool for Conway's Game of Life",
	long_description = """\
Python package intended to help in designing complex patterns for
Conway's Game of Life and related cellular automata.
Sample pattern-describing scripts and resulting patterns are included.
You may also want to install the 'plife' package to view them.""",
	author = "Eugene Langvagen",
	author_email = "elang@yandex.ru",
	url = "http://plife.sourceforge.net/",
	license = "GPL",
	packages = ["life"],
	package_dir = {"life": "python/life"},
	ext_modules = [Extension ("life.lifeint", [
		"src/life.cc", "src/life_io.cc",
		"src/life_rect.cc", "src/lifeint.cc"
		], libraries = ["stdc++"])],
	data_files = [
		(join (docdir, "samples"), glob (join ("samples", "*.py")) + glob (join ("samples", "*.lif"))),
		(join (docdir, "samples", "lib"), glob (join ("samples", "lib", "*.py")))
	]
)
