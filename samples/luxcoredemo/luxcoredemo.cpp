/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <iostream>

#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
//	printf("\n*** ");
//	if(fif != FIF_UNKNOWN)
//		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));
//
//	printf("%s", message);
//	printf(" ***\n");
//}

int main(int argc, char *argv[]) {
	try {
//		// Initialize FreeImage Library
//		FreeImage_Initialise(TRUE);
//		FreeImage_SetOutputMessage(FreeImageErrorHandler);

		cout << "LuxCore " << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n\n" ;

		//----------------------------------------------------------------------
		// Properties example
		//----------------------------------------------------------------------

		{
			cout << "Properties examples...\n";

			Property prop("test1.prop1", "aa");
			cout << "test1.prop1[0] => " << prop.Get<string>(0) << "\n\n";

			prop.Clear().Add(0).Add(2).Add(3);
			prop.Set(0, 1);
			cout << prop.ToString() << "\n\n";

			Properties props(
					(Property("test1.prop1") = 1, "aa", 3.f) %
					(Property("test2.prop2") = "test"));
			cout << props << "\n";

			cout << "test1.prop1[0] => " << props.Get("test1.prop1").Get<int>(0) << "\n";
			cout << "test1.prop1[1] => " << props.Get("test1.prop1").Get<string>(1) << "\n";
			cout << "test1.prop1[2] => " << props.Get("test1.prop1").Get<float>(2) << "\n\n";
			
			props.Clear().LoadFromString("test1.prop1 = 1 2.0 aa \"quoted\"\ntest2.prop2 = 1 2.0 'quoted' bb");
			cout << props;
			cout << "Size: " << props.Get("test1.prop1").GetSize() << "\n";
			cout << "Size: " << props.Get("test2.prop2").GetSize() << "\n";
		}
	} catch (runtime_error err) {
		cerr << "RUNTIME ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	} catch (exception err) {
		cerr << "ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}