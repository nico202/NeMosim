OPTION(NEMO_PYTHON_ENABLED "Create Python API bindings" ON)
IF(NEMO_PYTHON_ENABLED)
	subdirs(python)
ENDIF(NEMO_PYTHON_ENABLED)

OPTION(NEMO_MATLAB_ENABLED "Create Matlab API bindings" ON)
IF(NEMO_MATLAB_ENABLED)
	SUBDIRS(matlab)
ENDIF(NEMO_MATLAB_ENABLED)
