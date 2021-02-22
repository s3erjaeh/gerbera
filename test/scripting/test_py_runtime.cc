/*GRB*
    Gerbera - https://gerbera.io/

    runtime_test.cc - this file is part of Gerbera.

    Copyright (C) 2016-2021 Gerbera Contributors

    Gerbera is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    Gerbera is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gerbera.  If not, see <http://www.gnu.org/licenses/>.

    $Id$
*/
#ifdef HAVE_PY

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "content/scripting/py_runtime.h"
#include "test_py_runtime.h"
#include "content/content_manager.h"


TEST_F(PyRuntimeTest, CheckPythonScript_JPG)
{
    auto item = std::make_shared<CdsItem>();
    item->setLocation("/path/to/file.jpg");
    item->setMimeType("image/jpeg");
    item->setClass("object.item.imageItem");
    item->setMetadata(M_DATE, "2021-02-18");

    std::vector<std::string> result;
    runtime.processCdsObject(item, "/path", [this, &result](CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass) {
        result.push_back("/" + fmt::format("{}", fmt::join(chain,"/")));
        return 1;
    });
    
    ASSERT_THAT(result, testing::UnorderedElementsAre("/Pictures/Date/2021/02", "/Pictures/Directories/to"));
}

TEST_F(PyRuntimeTest, CheckPythonScript_Mp3)
{
    auto item = std::make_shared<CdsItem>();
    item->setLocation("/path/to/file.mp3");
    item->setMimeType("audio/mp3");
    item->setClass("object.item.audioItem.musicTrack");
    
    item->setMetadata(M_DATE, "2021-02-18");
    item->setMetadata(M_TITLE, "C++Titel");
    item->setMetadata(M_ARTIST, "C++Artist");
    item->setMetadata(M_ALBUM, "C++Album");
    item->setMetadata(M_GENRE, "C++Genre");
    item->setMetadata(M_COMPOSER, "C++Composer");
    item->setMetadata(M_CONDUCTOR, "C++Conductor");
    item->setMetadata(M_ORCHESTRA, "C++Orchestra");
    item->setMetadata(M_DESCRIPTION, "C++Descr");
    
    item->setTitle("EMPTY");

    std::vector<std::string> result;
    int checks = 0;
    runtime.processCdsObject(item, "/path", [this, &result, &checks](CdsObject& obj, const std::vector<std::string>& chain, std::string containerclass) {
        result.push_back("/" + fmt::format("{}", fmt::join(chain, "/")));

        if (chain.back().compare("All - full name") == 0 && obj.getTitle().compare("C++Artist - C++Album - C++Titel") == 0)
            checks++;
        if (containerclass.compare(UPNP_CLASS_MUSIC_ALBUM) == 0 && obj.getTitle().compare("C++Titel") == 0)
            checks++;

        return 1;
    });

    ASSERT_EQ(checks, 4);

    std::string expected[] = { 
        "/Audio/All Audio",
        "/Audio/Artists/C++Artist/All Songs",
        "/Audio/All - full name",
        "/Audio/Artists/C++Artist/All - full name",
        "/Audio/Artists/C++Artist/C++Album",
        "/Audio/Albums/C++Album",
        "/Audio/Genres/C++Genre",
        "/Audio/Year/2021",
        "/Audio/Composers/C++Composer"
    };
    ASSERT_THAT(result, testing::UnorderedElementsAreArray(expected));
}

#endif
