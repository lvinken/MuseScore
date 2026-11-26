//=============================================================================
//  MuseScore
//  Music Composition & Notation
//
//  Copyright (C) 2013 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

#include <QtTest/QtTest>
#include "libmscore/score.h"
#include "mtest/testutils.h"

#define DIR QString("tabledit/io/")

using namespace Ms;

//---------------------------------------------------------
//   TestTablEditIO
//---------------------------------------------------------

class TestTablEditIO : public QObject, public MTest
      {
      Q_OBJECT

      void tefReadTest(const char* file);

private slots:
      void initTestCase();

      // The list of TablEdit regression tests
      // Currently failing tests are commented out and annotated with the failure reason

      void tefBass() { tefReadTest("bass"); }
#if 0
      void capTest1() { capReadTest("test1"); }
      void capTestTuplet2() { capReadTest("testTuplet2"); } // generates different beaming with respect to the original
      void capxTestVolta1() { capxReadTest("testVolta1"); }
      void capxTestBarline() { capxReadTest("testBarline"); }
#endif
      };

//---------------------------------------------------------
//   initTestCase
//---------------------------------------------------------

void TestTablEditIO::initTestCase()
      {
      initMTest();
      }

//---------------------------------------------------------
//   tefReadTest
//   read a TablEdit file, write to a MuseScore file and verify against reference
//---------------------------------------------------------

void TestTablEditIO::tefReadTest(const char* file)
      {
      MasterScore* score = readScore(DIR + file + ".tef");
      QVERIFY(score);
      QVERIFY(saveCompareScore(score, QString("%1.tef.mscx").arg(file),
                               DIR + QString("%1.tef-ref.mscx").arg(file)));
      delete score;
      }

QTEST_MAIN(TestTablEditIO)
#if __has_include("tst_tabledit_io.moc")
#include "tst_tabledit_io.moc"
#endif
