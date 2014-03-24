/**@author Merder Kim <hoxnox@gmail.com>
 * @date 20140324 20:20:30
 *
 * @brief csio test launcher.*/

// Google Testing Framework
#include <gtest/gtest.h>
#include "tcsio_none.hpp"
#include "tcsio_dictzip.hpp"
#include "tcqueue.hpp"

// test cases

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


