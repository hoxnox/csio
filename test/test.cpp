/**@author $username$ <$usermail$>
 * @date $date$
 *
 * @brief csio test launcher.*/

// Google Testing Framework
#include <gtest/gtest.h>
#include "tcsio_none.hpp"
#include "tcsio_dictzip.hpp"

// test cases

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


