//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2015 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#include <fstream>

#include "libmscore/box.h"
#include "libmscore/measure.h"
#include "libmscore/page.h"
#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "libmscore/sym.h"
#include "libmscore/symbol.h"

#include "importmxml.h"
#include "importmxmllogger.h"
#include "importmxmlpass1.h"
#include "importmxmlpass2.h"
#include "musicxml.hxx"

#include "mscore/preferences.h"

namespace Ms {

Score::FileError importMusicXMLfromBuffer(Score* score, const QString& name, QIODevice* dev)

      {
      qDebug("importMusicXMLfromBuffer(score %p, name '%s', dev %p) 1a",
             score, qPrintable(name), dev);
      qDebug("importMusicXMLfromBuffer() 1b QDir::currentPath() '%s'",
             qPrintable(QDir::currentPath()));

      MxmlLogger logger;
      logger.setLoggingLevel(MxmlLogger::Level::MXML_ERROR); // errors only
      //logger.setLoggingLevel(MxmlLogger::Level::MXML_INFO);
      //logger.setLoggingLevel(MxmlLogger::Level::MXML_TRACE); // also include tracing

      dev->seek(0);
      MusicXMLParserPass1 pass1(score, &logger);

      // pass 0 :-)
      std::fstream fs(name.toStdString(), std::ios::in);
      if (!fs.is_open()) {
          qDebug("importMusicXMLfromBuffer() 2 could not open MusicXML file '%s'", qPrintable(name));
          MScore::lastError = QObject::tr("Could not open MusicXML file\n%1").arg(name);
          return Score::FileError::FILE_OPEN_ERROR;
      }
      qDebug("importMusicXMLfromBuffer() 3 successfully opened MusicXML file '%s'", qPrintable(name));

      Score::FileError res { Score::FileError::FILE_NO_ERROR };

      try {
          const auto score_partwise = musicxml::score_partwise_(fs);
          qDebug("importMusicXMLfromBuffer(): 4 score_partwise created");
          res = pass1.parse(*score_partwise);
          if (res != Score::FileError::FILE_NO_ERROR)
                return res;
      } catch (const xml_schema::exception &e) {
          std::cerr << "importMusicXMLfromBuffer(): 5a xml_schema::exception " << e << std::endl;
          qDebug("importMusicXMLfromBuffer(): 5b xml_schema::exception");
          return Score::FileError::FILE_BAD_FORMAT;
      } catch (const std::ios_base::failure &) {
          qDebug("importMusicXMLfromBuffer(): 6 unable to open or read failure");
          return Score::FileError::FILE_OPEN_ERROR;
      }

#if 0
      // pass 1
      dev->seek(0);
      res = pass1.parse(dev);
      if (res != Score::FileError::FILE_NO_ERROR)
            return res;
#endif

      // pass 2
      dev->seek(0);
      MusicXMLParserPass2 pass2(score, pass1, &logger);
      return pass2.parse(dev);
      }

} // namespace Ms
