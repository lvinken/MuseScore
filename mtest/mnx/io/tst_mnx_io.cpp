//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2017 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

// Note: this file is currently unfinished. It does not yet work in the mtest framework,
// but serves as a data file for the iotest in this directory.

#include <QtTest/QtTest>
#include "mtest/testutils.h"
#include "libmscore/score.h"
#include "mscore/preferences.h"
// start includes required for fixupScore()
#include "libmscore/measure.h"
#include "libmscore/staff.h"
#include "libmscore/keysig.h"
// end includes required for fixupScore()

namespace Ms {
      extern bool saveMxl(Score*, const QString&);
      }

#define DIR QString("mnx/io/")

using namespace Ms;

//---------------------------------------------------------
//   TestMnxIO
//---------------------------------------------------------

class TestMnxIO : public QObject, public MTest
      {
      Q_OBJECT

      void mnxIoTest(const char* file);
      void mnxIoTestRef(const char* file);


      // The list of MNX regression tests
      // Currently failing tests are commented out and annotated with the failure reason
      // To extract the list in a shell scipt use:
      // cat tst_mnx_io.cpp | grep "{ <test>" | awk -F\" '{print $2}'
      // where <test> is mnxIoTest

private slots:
      void initTestCase();

      void hello() { mnxIoTest("testHello"); }
      void notesRests1() { mnxIoTest("testNotesRests1"); }
      };

//---------------------------------------------------------
//   initTestCase
//---------------------------------------------------------

void TestMnxIO::initTestCase()
      {
      initMTest();
      }

//---------------------------------------------------------
//   fixupScore -- do required fixups after MusicXML import
//   see mscore/file.cpp MuseScore::readScore(Score* score, QString name)
//   TODO: remove duplication of code
//---------------------------------------------------------

static void fixupScore(Score* score)
      {
//      score->syntiState().append(SyntiParameter("soundfont", MScore::soundFont));
      score->connectTies();
      score->rebuildMidiMapping();
      score->setCreated(false);
      score->setSaved(false);

#if 0
      int staffIdx = 0;
      foreach(Staff* st, score->staves()) {
            if (st->updateKeymap())
                  st->clearKeys();
            int track = staffIdx * VOICES;
            KeySig* key1 = 0;
            for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
                  for (Segment* s = m->first(); s; s = s->next()) {
                        if (!s->element(track))
                              continue;
                        Element* e = s->element(track);
                        if (e->generated())
                              continue;
                        //if ((s->subtype() == SegClef) && st->updateClefList()) {
                        //      Clef* clef = static_cast<Clef*>(e);
                        //      st->setClef(s->tick(), clef->clefTypeList());
                        //      }
                        if ((s->segmentType() == Segment::Type::KeySig) && st->updateKeymap()) {
                              KeySig* ks = static_cast<KeySig*>(e);
                              int naturals = key1 ? key1->keySigEvent().accidentalType() : 0;
                              ks->setOldSig(naturals);
                              st->setKey(s->tick(), ks->key());
                              key1 = ks;
                              }
                        }
                  if (m->sectionBreak())
                        key1 = 0;
                  }
            st->setUpdateKeymap(false);
            ++staffIdx;
            }
#endif

      score->updateNotes();
      

//---------------------------------------------------------
//   IoTest
//   read a BWW file, write to a new file and verify against MusicXML reference
//---------------------------------------------------------

void TestMnxIO::IoTest(const char* file)
      {
      MScore::debugMode = true;
      preferences.musicxmlExportBreaks = MusicxmlExportBreaks::MANUAL;
      preferences.musicxmlImportBreaks = true;
      Score* score = readScore(DIR + file + ".xml");
      QVERIFY(score);
      fixupScore(score);
      score->doLayout();
      QVERIFY(saveMusicXml(score, QString(file) + ".xml"));
      QVERIFY(saveCompareMusicXmlScore(score, QString(file) + ".xml", DIR + file + ".xml"));
      delete score;
      }

//---------------------------------------------------------
//   IoTestRef
//   read a MusicXML file, write to a new file and verify against reference
//---------------------------------------------------------

void TestMnxIO::IoTestRef(const char* file)
      {
      MScore::debugMode = true;
      preferences.musicxmlExportBreaks = MusicxmlExportBreaks::MANUAL;
      preferences.musicxmlImportBreaks = true;
      Score* score = readScore(DIR + file + ".xml");
      QVERIFY(score);
      fixupScore(score);
      score->doLayout();
      QVERIFY(saveMusicXml(score, QString(file) + ".xml"));
      QVERIFY(saveCompareMusicXmlScore(score, QString(file) + ".xml", DIR + file + "_ref.xml"));
      delete score;
      }

QTEST_MAIN(TestMnxIO)
#include "tst__io.moc"
