/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_BITMAPUTILS_H_
#define _CARTO_BITMAPUTILS_H_

#include <memory>
#include <string>

namespace carto {
    class Bitmap;

    /**
     * A helper class for loading bitmaps.
     */
    class BitmapUtils {
    public:
        /**
         * Loads the specified bitmap asset bundled with the application.
         * @param assetPath The asset path to the image to be loaded.
         */
        static std::shared_ptr<Bitmap> LoadBitmapFromAssets(const std::string& assetPath);
        
        /**
         * Loads bitmap from specified file.
         * @param filePath The path to the image to be loaded.
         */
        static std::shared_ptr<Bitmap> LoadBitmapFromFile(const std::string& filePath);

    protected:
        BitmapUtils();
    };

}

#endif
