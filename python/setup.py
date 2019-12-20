from setuptools import setup, find_packages

setup(name="pypspproxy",
      version="0.1",
      description="libpspproxy python bindings",
      author="Alexander Eichner",
      py_modules=["pypspproxy"],
      setup_requires=["cffi"],
      cffi_modules=["build_pypspproxy.py:ffibuilder"],
      install_requires=["cffi"],
)

