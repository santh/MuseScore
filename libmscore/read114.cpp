//=============================================================================
//  MuseScore
//  Music Composition & Notation
//  $Id:$
//
//  Copyright (C) 2011 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENSE.GPL
//=============================================================================

#include "score.h"
#include "slur.h"
#include "staff.h"
#include "excerpt.h"
#include "chord.h"
#include "rest.h"
#include "keysig.h"
#include "volta.h"
#include "measure.h"
#include "beam.h"
#include "page.h"
#include "segment.h"
#include "ottava.h"
#include "stafftype.h"
#include "text.h"
#include "part.h"
#include "sig.h"
#include "box.h"
#include "dynamic.h"
#include "drumset.h"

//---------------------------------------------------------
//   resolveSymCompatibility
//---------------------------------------------------------

static SymId resolveSymCompatibility(SymId i, QString programVersion)
      {
      if(!programVersion.isEmpty() && programVersion < "1.1")
          i = SymId(i + 5);
      switch(i) {
            case 197:
                  return pedalPedSym;
            case 191:
                  return pedalasteriskSym;
            case 193:
                  return pedaldotSym;
            case 192:
                  return pedaldashSym;
            case 139:
                  return trillSym;
            default:
                  return noSym;
            }
      }

//---------------------------------------------------------
//   Staff::read114
//---------------------------------------------------------

void Staff::read114(XmlReader& e, ClefList& _clefList)
      {
      while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());
            if (tag == "type") {
                  StaffType* st = score()->staffType(e.readInt());
                  if (st)
                        _staffType = st;
                  }
            else if (tag == "lines")
                  setLines(e.readInt());
            else if (tag == "small")
                  setSmall(e.readInt());
            else if (tag == "invisible")
                  setInvisible(e.readInt());
            else if (tag == "slashStyle")
                  e.skipCurrentElement();       // obsolete: setSlashStyle(v);
            else if (tag == "cleflist")
                  _clefList.read(e, _score);
            else if (tag == "keylist")
                  _keymap->read(e, _score);
            else if (tag == "bracket") {
                  BracketItem b;
                  b._bracket = BracketType(e.intAttribute("type", -1));
                  b._bracketSpan = e.intAttribute("span", 0);
                  _brackets.append(b);
                  e.readNext();
                  }
            else if (tag == "barLineSpan")
                  _barLineSpan = e.readInt();
            else if (tag == "distOffset")
                  _userDist = e.readDouble() * spatium();
            else if (tag == "linkedTo") {
                  int v = e.readInt() - 1;
                  //
                  // if this is an excerpt, link staff to parentScore()
                  //
                  if (score()->parentScore()) {
                        Staff* st = score()->parentScore()->staff(v);
                        if (st)
                              linkTo(st);
                        else {
                              qDebug("staff %d not found in parent", v);
                              }
                        }
                  else {
                        int idx = score()->staffIdx(this);
                        if (v < idx)
                              linkTo(score()->staff(v));
                        }
                  }
            else
                  e.unknown();
            }
      }

//---------------------------------------------------------
//   Part::read114
//---------------------------------------------------------

void Part::read114(XmlReader& e, QList<ClefList*>& clefList)
      {
      int rstaff = 0;
      while (e.readNextStartElement()) {
            const QStringRef& tag(e.name());
            if (tag == "Staff") {
                  Staff* staff = new Staff(_score, this, rstaff);
                  _score->staves().push_back(staff);
                  _staves.push_back(staff);
                  ClefList* cl = new ClefList;
                  clefList.append(cl);
                  staff->read114(e, *cl);
                  ++rstaff;
                  }
            else if (tag == "Instrument") {
                  instr(0)->read(e);
                  Drumset* d = instr(0)->drumset();
                  Staff*   st = staff(0);
                  if (d && st && st->lines() != 5) {
                        int n = 0;
                        if (st->lines() == 1)
                              n = 4;
                        for (int  i = 0; i < DRUM_INSTRUMENTS; ++i)
                              d->drum(i).line -= n;
                        }
                  }
            else if (tag == "name") {
                  Text* t = new Text(score());
                  t->read(e);
                  instr(0)->setLongName(t->getFragment());
                  delete t;
                  }
            else if (tag == "shortName") {
                  Text* t = new Text(score());
                  t->read(e);
                  instr(0)->setShortName(t->getFragment());
                  delete t;
                  }
            else if (tag == "trackName")
                  _partName = e.readElementText();
            else if (tag == "show")
                  _show = e.readInt();
            else
                  e.unknown();
            }
      if (_partName.isEmpty())
            _partName = instr(0)->trackName();

      if (instr(0)->useDrumset()) {
            foreach(Staff* staff, _staves) {
                  int lines = staff->lines();
                  staff->setStaffType(score()->staffType(PERCUSSION_STAFF_TYPE));
                  staff->setLines(lines);
                  }
            }
      }


//---------------------------------------------------------
//   read114
//    import old version 1.2 files
//    return false on error
//---------------------------------------------------------

Score::FileError Score::read114(XmlReader& e)
      {
      QList<Spanner*> spannerList;
      QList<ClefList*> clefListList;

      if (parentScore())
            setMscVersion(parentScore()->mscVersion());

      while (e.readNextStartElement()) {
            e.setTrack(-1);
            const QStringRef& tag(e.name());
            if (tag == "Staff")
                  readStaff(e);
            else if (tag == "KeySig") {
                  KeySig* ks = new KeySig(this);
                  ks->read(e);
                  customKeysigs.append(ks);
                  }
            else if (tag == "StaffType") {
                  int idx        = e.intAttribute("idx");
                  StaffType* ost = staffType(idx);
                  StaffType* st;
                  if (ost)
                        st = ost->clone();
                  else {
                        QString group  = e.attribute("group", "pitched");
                        if (group == "percussion")
                              st  = new StaffTypePercussion();
                        else if (group == "tablature")
                              st  = new StaffTypeTablature();
                        else
                              st  = new StaffTypePitched();
                        }
                  st->read(e);
                  st->setBuildin(false);
                  addStaffType(idx, st);
                  }
            else if (tag == "siglist")
                  _sigmap->read(e, _fileDivision);
            else if (tag == "tempolist")        // obsolete
                  e.skipCurrentElement();       // tempomap()->read(ee, _fileDivision);
            else if (tag == "programVersion") {
                  _mscoreVersion = e.readElementText();
                  parseVersion(_mscoreVersion);
                  }
            else if (tag == "programRevision")
                  _mscoreRevision = e.readInt();
            else if (tag == "Mag" || tag == "MagIdx" || tag == "xoff" || tag == "yoff")
                  e.skipCurrentElement();       // obsolete
            else if (tag == "playMode")
                  _playMode = PlayMode(e.readInt());
            else if (tag == "SyntiSettings") {
                  _syntiState.clear();
                  _syntiState.read(e);
                  }
            else if (tag == "Spatium")
                  _style.setSpatium (e.readDouble() * MScore::DPMM); // obsolete, moved to Style
            else if (tag == "page-offset")            // obsolete, moved to Score
                  setPageNumberOffset(e.readInt());
            else if (tag == "Division")
                  _fileDivision = e.readInt();
            else if (tag == "showInvisible")
                  _showInvisible = e.readInt();
            else if (tag == "showUnprintable")
                  _showUnprintable = e.readInt();
            else if (tag == "showFrames")
                  _showFrames = e.readInt();
            else if (tag == "showMargins")
                  _showPageborders = e.readInt();
            else if (tag == "Style") {
                  qreal sp = _style.spatium();
                  _style.load(e);
                  if (_layoutMode == LayoutFloat) {
                        // style should not change spatium in
                        // float mode
                        _style.setSpatium(sp);
                        }
                  }
            else if (tag == "TextStyle") {
                  TextStyle s;
                  s.read(e);
                  // settings for _reloff::x and _reloff::y in old formats
                  // is now included in style; setting them to 0 fixes most
                  // cases of backward compatibility
                  s.setRxoff(0);
                  s.setRyoff(0);
                  _style.setTextStyle(s);
                  }
            else if (tag == "page-layout") {
                  if (_layoutMode != LayoutFloat && _layoutMode != LayoutSystem) {
                        PageFormat pf;
                        pf.copy(*pageFormat());
                        pf.read(e);
                        setPageFormat(pf);
                        }
                  else
                        e.skipCurrentElement();
                  }
            else if (tag == "copyright" || tag == "rights") {
                  Text* text = new Text(this);
                  text->read(e);
                  setMetaTag("copyright", text->getText());
                  delete text;
                  }
            else if (tag == "movement-number")
                  setMetaTag("movementNumber", e.readElementText());
            else if (tag == "movement-title")
                  setMetaTag("movementTitle", e.readElementText());
            else if (tag == "work-number")
                  setMetaTag("workNumber", e.readElementText());
            else if (tag == "work-title")
                  setMetaTag("workTitle", e.readElementText());
            else if (tag == "source")
                  setMetaTag("source", e.readElementText());
            else if (tag == "metaTag") {
                  QString name = e.attribute("name");
                  setMetaTag(name, e.readElementText());
                  }
            else if (tag == "Part") {
                  Part* part = new Part(this);
                  part->read114(e, clefListList);
                  _parts.push_back(part);
                  }
            else if (tag == "Symbols" || tag == "cursorTrack")          // obsolete
                  e.skipCurrentElement();
            else if (tag == "Slur") {
                  Slur* slur = new Slur(this);
                  slur->read(e);
                  e.addSpanner(slur);
                  }
            else if ((tag == "HairPin")
                || (tag == "Ottava")
                || (tag == "TextLine")
                || (tag == "Volta")
                || (tag == "Trill")
                || (tag == "Pedal")) {
                  Spanner* s = static_cast<Spanner*>(Element::name2Element(tag, this));
                  s->setTrack(0);
                  s->read(e);
                  spannerList.append(s);
                  }
            else if (tag == "Excerpt") {
                  Excerpt* ex = new Excerpt(this);
                  ex->read(e);
                  _excerpts.append(ex);
                  }
            else if (tag == "Beam") {
                  Beam* beam = new Beam(this);
                  beam->read(e);
                  beam->setParent(0);
                  // _beams.append(beam);
                  }
            else if (tag == "Score") {          // recursion
                  Score* s = new Score(style());
                  s->setParentScore(this);
                  s->read(e);
                  addExcerpt(s);
                  }
            else if (tag == "PageList") {
                  while (e.readNextStartElement()) {
                        if (e.name() == "Page") {
                              Page* page = new Page(this);
                              _pages.append(page);
                              page->read(e);
                              }
                        else
                              e.unknown();
                        }
                  }
            else if (tag == "name")
                  setName(e.readElementText());
            else
                  e.unknown();
            }

      for (int idx = 0; idx < _staves.size(); ++idx) {
            Staff* s = _staves[idx];
            int track = idx * VOICES;

            ClefList* cl = clefListList[idx];
            for (ciClefEvent i = cl->constBegin(); i != cl->constEnd(); ++i) {
                  int tick = i.key();
                  ClefType clefId = i.value()._concertClef;
                  Measure* m = tick2measure(tick);
                  if ((tick == m->tick()) && m->prevMeasure())
                        m = m->prevMeasure();
                  Segment* seg = m->getSegment(Segment::SegClef, tick);
                  if (seg->element(track))
                        static_cast<Clef*>(seg->element(track))->setGenerated(false);
                  else {
                        Clef* clef = new Clef(this);
                        clef->setClefType(clefId);
                        clef->setTrack(track);
                        clef->setParent(seg);
                        clef->setGenerated(false);
                        seg->add(clef);
                        }
                  }

            KeyList* km = s->keymap();
            for (ciKeyList i = km->begin(); i != km->end(); ++i) {
                  int tick = i->first;
                  KeySigEvent ke = i->second;
                  Measure* m = tick2measure(tick);
                  Segment* seg = m->getSegment(Segment::SegKeySig, tick);
                  if (seg->element(track))
                        static_cast<KeySig*>(seg->element(track))->setGenerated(false);
                  else {
                        KeySig* ks = keySigFactory(ke);
                        if (ks) {
                              ks->setParent(seg);
                              ks->setTrack(track);
                              ks->setGenerated(false);
                              seg->add(ks);
                              }
                        }
                  }
            }
      qDeleteAll(clefListList);

      foreach(Spanner* s, spannerList) {
            s->setTrack(0);

            if (s->type() == Element::OTTAVA
                  || (s->type() == Element::TEXTLINE)
                  || (s->type() == Element::VOLTA)
                  || (s->type() == Element::PEDAL)) {
                      TextLine* tl = static_cast<TextLine*>(s);
                      tl->setBeginSymbol(resolveSymCompatibility(tl->beginSymbol(), mscoreVersion()));
                      tl->setContinueSymbol(resolveSymCompatibility(tl->continueSymbol(), mscoreVersion()));
                      tl->setEndSymbol(resolveSymCompatibility(tl->endSymbol(), mscoreVersion()));
                      }

            int tick2 = s->__tick2();
            Segment* s1 = tick2segment(e.tick());
            Segment* s2 = tick2segment(tick2);
            if (s1 == 0 || s2 == 0) {
                  qDebug("cannot place %s at tick %d - %d",
                     s->name(), s->__tick1(), tick2);
                  continue;
                  }
            if (s->type() == Element::VOLTA) {
                  Volta* volta = static_cast<Volta*>(s);
                  volta->setAnchor(Spanner::ANCHOR_MEASURE);
                  volta->setStartMeasure(s1->measure());
                  Measure* m2 = s2->measure();
                  if (s2->tick() == m2->tick())
                        m2 = m2->prevMeasure();
                  volta->setEndMeasure(m2);
                  s1->measure()->add(s);
                  int n = volta->spannerSegments().size();
                  if (n) {
                        // volta->setYoff(-styleS(ST_voltaHook).val());
                        // LineSegment* ls = volta->segmentAt(0);
                        // ls->setReadPos(QPointF());
                        }
                  }
            else {
                  s->setStartElement(s1);
                  s->setEndElement(s2);
                  s1->add(s);
                  }
            if (s->type() == Element::OTTAVA) {
                  // fix ottava position
                  Ottava* volta = static_cast<Ottava*>(s);
                  int n = volta->spannerSegments().size();
                  for (int i = 0; i < n; ++i) {
                        LineSegment* seg = volta->segmentAt(i);
                        if (!seg->userOff().isNull())
                              seg->setUserYoffset(seg->userOff().y() - styleP(ST_ottavaY));
                        }
                  }
            }

      // check slurs
      foreach(Spanner* s, e.spanner()) {
            if (s->type() != Element::SLUR)
                  continue;
            Slur* slur = static_cast<Slur*>(s);

            if (!slur->startElement() || !slur->endElement()) {
                  qDebug("incomplete Slur");
                  if (slur->startElement()) {
                        qDebug("  front %d", static_cast<ChordRest*>(slur->startElement())->tick());
                        static_cast<ChordRest*>(slur->startElement())->removeSlurFor(slur);
                        }
                  if (slur->endElement()) {
                        qDebug("  back %d", static_cast<ChordRest*>(slur->endElement())->tick());
                        static_cast<ChordRest*>(slur->endElement())->removeSlurBack(slur);
                        }
                  }
            else {
                  ChordRest* cr1 = (ChordRest*)(slur->startElement());
                  ChordRest* cr2 = (ChordRest*)(slur->endElement());
                  if (cr1->tick() > cr2->tick()) {
                        qDebug("Slur invalid start-end tick %d-%d", cr1->tick(), cr2->tick());
                        slur->setStartElement(cr2);
                        slur->setEndElement(cr1);
                        }
                  int n1 = 0;
                  int n2 = 0;
                  for (Spanner* s = cr1->spannerFor(); s; s = s->next()) {
                        if (s == slur)
                              ++n1;
                        }
                  for (Spanner* s = cr2->spannerBack(); s; s = s->next()) {
                        if (s == slur)
                              ++n2;
                        }
                  if (n1 != 1 || n2 != 1) {
                        qDebug("Slur references bad: %d %d", n1, n2);
                        }
                  }
            }
      connectTies();

      //
      // remove "middle beam" flags from first ChordRest in
      // measure
      //
      for (Measure* m = firstMeasure(); m; m = m->nextMeasure()) {
            int tracks = nstaves() * VOICES;
            for (int track = 0; track < tracks; ++track) {
                  for (Segment* s = m->first(); s; s = s->next()) {
                        if (s->subtype() != Segment::SegChordRest)
                              continue;
                        ChordRest* cr = static_cast<ChordRest*>(s->element(track));
                        if (cr) {
                              switch(cr->beamMode()) {
                                    case BEAM_AUTO:
                                    case BEAM_BEGIN:
                                    case BEAM_END:
                                    case BEAM_NO:
                                          break;
                                    case BEAM_MID:
                                    case BEAM_BEGIN32:
                                    case BEAM_BEGIN64:
                                          cr->setBeamMode(BEAM_BEGIN);
                                          break;
                                    case BEAM_INVALID:
                                          if (cr->type() == Element::CHORD)
                                                cr->setBeamMode(BEAM_AUTO);
                                          else
                                                cr->setBeamMode(BEAM_NO);
                                          break;
                                    }
                              break;
                              }
                        }
                  }
            }
      for (MeasureBase* mb = _measures.first(); mb; mb = mb->next()) {
            if (mb->type() == Element::VBOX) {
                  Box* b  = static_cast<Box*>(mb);
                  qreal y = point(styleS(ST_staffUpperBorder));
                  b->setBottomGap(y);
                  }
            }

      _fileDivision = MScore::division;

      //
      //    sanity check for barLineSpan
      //
      foreach(Staff* staff, _staves) {
            int barLineSpan = staff->barLineSpan();
            int idx = staffIdx(staff);
            int n = nstaves();
            if (idx + barLineSpan > n) {
                  qDebug("bad span: idx %d  span %d staves %d", idx, barLineSpan, n);
                  staff->setBarLineSpan(n - idx);
                  }
            }

      // adjust some styles
      if (styleS(ST_lyricsDistance).val() == MScore::baseStyle()->valueS(ST_lyricsDistance).val())
            style()->set(ST_lyricsDistance, Spatium(2.0));
      if (styleS(ST_voltaY).val() == MScore::baseStyle()->valueS(ST_voltaY).val())
            style()->set(ST_voltaY, Spatium(-2.0));
      if (styleB(ST_hideEmptyStaves) == true) // http://musescore.org/en/node/16228
            style()->set(ST_dontHideStavesInFirstSystem, false);

      _showOmr = false;

      // create excerpts
      foreach(Excerpt* excerpt, _excerpts) {
            Score* nscore = ::createExcerpt(excerpt->parts());
            if (nscore) {
                  nscore->setParentScore(this);
                  nscore->setName(excerpt->title());
                  nscore->rebuildMidiMapping();
                  nscore->updateChannel();
                  nscore->addLayoutFlags(LAYOUT_FIX_PITCH_VELO);
                  nscore->doLayout();
                  excerpt->setScore(nscore);
                  }
            }

      //
      // check for soundfont,
      // add default soundfont if none found
      // (for compatibility with old scores)
      //
      bool hasSoundfont = false;
      foreach(const SyntiParameter& sp, _syntiState) {
            if (sp.name() == "soundfont") {
                  QFileInfo fi(sp.sval());
                  if (fi.exists())
                        hasSoundfont = true;
                  }
            }
      if (!hasSoundfont)
            _syntiState.append(SyntiParameter("soundfont", MScore::soundFont));

//      _mscVersion = MSCVERSION;     // for later drag & drop usage
      fixTicks();
      renumberMeasures();
      rebuildMidiMapping();
      updateChannel();
      updateNotes();    // only for parts needed?

      doLayout();

      //
      // move some elements
      //
      for (Segment* s = firstSegment(); s; s = s->next1()) {
            foreach (Element* e, s->annotations()) {
                  if (e->type() == Element::TEMPO_TEXT) {
                        // reparent from measure to segment
                        e->setUserOff(QPointF(e->userOff().x() - s->pos().x(),
                           e->userOff().y()));
                        }
                  }
            }

      return FILE_NO_ERROR;
      }
