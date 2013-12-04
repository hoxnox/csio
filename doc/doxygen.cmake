# doxygen helper

set(ENV{csio_ROOT} ${PDIR})
message(${PDIR})
execute_process(
	COMMAND doxygen "${PDIR}/doc/doxygen")
