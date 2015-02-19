#include "output_dash_mp4.h"
#include <mist/defines.h>
#include <mist/mp4.h>
#include <mist/mp4_generic.h>
#include <mist/mp4_dash.h>
#include <mist/checksum.h>

namespace Mist {
  OutDashMP4::OutDashMP4(Socket::Connection & conn) : HTTPOutput(conn){realTime = 0;}
  OutDashMP4::~OutDashMP4(){}
  
  std::string OutDashMP4::makeTime(long long unsigned int time){
    std::stringstream r;
    r << "PT" << (((time / 1000) / 60) /60) << "H" << ((time / 1000) / 60) % 60 << "M" << (time / 1000) % 60 << "." << time % 1000 / 10 << "S";
    return r.str();
  }
  
  void OutDashMP4::buildFtyp(unsigned int tid){
    H.Chunkify("\000\000\000", 3, myConn);
    H.Chunkify("\040", 1, myConn);
    H.Chunkify("ftypisom\000\000\000\000isom", 16, myConn);
    if (myMeta.tracks[tid].type == "video"){
      H.Chunkify("avc1", 4, myConn);
    }else{
      H.Chunkify("M4A ", 4, myConn);
    }
    H.Chunkify("mp42dash", 8, myConn);
  }

  void OutDashMP4::buildStyp(unsigned int tid){
    H.Chunkify("\000\000\000\030stypmsdh\000\000\000\000msdhmsix", 24, myConn);
  }
  
  std::string OutDashMP4::buildMoov(unsigned int tid){
    std::string trackType = myMeta.tracks[tid].type;
    MP4::MOOV moovBox;
    
    MP4::MVHD mvhdBox(0);
    mvhdBox.setTrackID(2);
    mvhdBox.setDuration(0xFFFFFFFF);
    moovBox.setContent(mvhdBox, 0);

    MP4::IODS iodsBox;
    if (trackType == "video"){
      iodsBox.setODVideoLevel(0xFE);
    }else{
      iodsBox.setODAudioLevel(0xFE);
    }
    moovBox.setContent(iodsBox, 1);
    
    
    MP4::MVEX mvexBox;
    MP4::MEHD mehdBox;
    mehdBox.setFragmentDuration(0xFFFFFFFF);
    mvexBox.setContent(mehdBox, 0);
    MP4::TREX trexBox;
    trexBox.setTrackID(1);
    mvexBox.setContent(trexBox, 1);
    moovBox.setContent(mvexBox, 2);
    
    MP4::TRAK trakBox;
    MP4::TKHD tkhdBox(1, 0, myMeta.tracks[tid].width, myMeta.tracks[tid].height);
    tkhdBox.setFlags(3);
    if (trackType == "audio"){
      tkhdBox.setVolume(256);
      tkhdBox.setWidth(0);
      tkhdBox.setHeight(0);
    }
    tkhdBox.setDuration(0xFFFFFFFF);
    trakBox.setContent(tkhdBox, 0);
    
    MP4::MDIA mdiaBox;
    MP4::MDHD mdhdBox(0);
    mdhdBox.setLanguage(0x44);
    mdhdBox.setDuration(myMeta.tracks[tid].lastms);
    mdiaBox.setContent(mdhdBox, 0);
    
    if (trackType == "video"){
      MP4::HDLR hdlrBox(myMeta.tracks[tid].type,"VideoHandler");
      mdiaBox.setContent(hdlrBox, 1);
    }else{
      MP4::HDLR hdlrBox(myMeta.tracks[tid].type,"SoundHandler");
      mdiaBox.setContent(hdlrBox, 1);
    }

    MP4::MINF minfBox;
    MP4::DINF dinfBox;
    MP4::DREF drefBox;
    dinfBox.setContent(drefBox, 0);
    minfBox.setContent(dinfBox, 0);
    
    MP4::STBL stblBox;
    MP4::STSD stsdBox;
    stsdBox.setVersion(0);
    
    if (myMeta.tracks[tid].codec == "H264"){
      MP4::AVC1 avc1Box;
      avc1Box.setWidth(myMeta.tracks[tid].width);
      avc1Box.setHeight(myMeta.tracks[tid].height);
      
      MP4::AVCC avccBox;
      avccBox.setPayload(myMeta.tracks[tid].init);
      avc1Box.setCLAP(avccBox);
      stsdBox.setEntry(avc1Box, 0);
    }
    if (myMeta.tracks[tid].codec == "HEVC"){
      MP4::HEV1 hev1Box;
      hev1Box.setWidth(myMeta.tracks[tid].width);
      hev1Box.setHeight(myMeta.tracks[tid].height);
      
      MP4::HVCC hvccBox;
      hvccBox.setPayload(myMeta.tracks[tid].init);
      hev1Box.setCLAP(hvccBox);
      stsdBox.setEntry(hev1Box, 0);
    }
    if (myMeta.tracks[tid].codec == "AAC"){
      MP4::AudioSampleEntry ase;
      ase.setCodec("mp4a");
      ase.setDataReferenceIndex(1);
      ase.setSampleRate(myMeta.tracks[tid].rate);
      ase.setChannelCount(myMeta.tracks[tid].channels);
      ase.setSampleSize(myMeta.tracks[tid].size);
      MP4::ESDS esdsBox(myMeta.tracks[tid].init);
      ase.setCodecBox(esdsBox);
      stsdBox.setEntry(ase,0);
    }
    if (myMeta.tracks[tid].codec == "AC3"){
      ///\todo Note: this code is copied, note for muxing seperation
      MP4::AudioSampleEntry ase;
      ase.setCodec("ac-3");
      ase.setDataReferenceIndex(1);
      ase.setSampleRate(myMeta.tracks[tid].rate);
      ase.setChannelCount(myMeta.tracks[tid].channels);
      ase.setSampleSize(myMeta.tracks[tid].size);
      MP4::DAC3 dac3Box;
      switch (myMeta.tracks[tid].rate){
        case 48000:
          dac3Box.setSampleRateCode(0);
          break;
        case 44100:
          dac3Box.setSampleRateCode(1);
          break;
        case 32000:
          dac3Box.setSampleRateCode(2);
          break;
        default:
          dac3Box.setSampleRateCode(3);
          break;
      }
      /// \todo the next settings are set to generic values, we might want to make these flexible
      dac3Box.setBitStreamIdentification(8);//check the docs, this is a weird property
      dac3Box.setBitStreamMode(0);//set to main, mixed audio
      dac3Box.setAudioConfigMode(2);///\todo find out if ACMode should be different
      if (myMeta.tracks[tid].channels > 4){
        dac3Box.setLowFrequencyEffectsChannelOn(1);
      }else{
        dac3Box.setLowFrequencyEffectsChannelOn(0);
      }
      dac3Box.setFrameSizeCode(20);//should be OK, but test this.
      ase.setCodecBox(dac3Box);
    }
    
    stblBox.setContent(stsdBox, 0);
    
    MP4::STTS sttsBox;
    sttsBox.setVersion(0);
    stblBox.setContent(sttsBox, 1);
    
    MP4::STSC stscBox;
    stscBox.setVersion(0);
    stblBox.setContent(stscBox, 2);
    
    MP4::STCO stcoBox;
    stcoBox.setVersion(0);
    stblBox.setContent(stcoBox, 3);
    
    MP4::STSZ stszBox;
    stszBox.setVersion(0);
    stblBox.setContent(stszBox, 4);
    
    minfBox.setContent(stblBox, 1);
    
    if (trackType == "video"){
      MP4::VMHD vmhdBox;
      vmhdBox.setFlags(1);
      minfBox.setContent(vmhdBox, 2);
    }else{
      MP4::SMHD smhdBox;
      minfBox.setContent(smhdBox, 2);
    }
    
    mdiaBox.setContent(minfBox, 2);
    
    trakBox.setContent(mdiaBox, 1);
    
    moovBox.setContent(trakBox, 3);
    
    return std::string(moovBox.asBox(),moovBox.boxedSize());
  }
    
  std::string OutDashMP4::buildSidx(unsigned int tid){
    MP4::AVCC avccBox;
    MP4::HVCC hvccBox;
    if (myMeta.tracks[tid].codec == "H264"){
      avccBox.setPayload(myMeta.tracks[tid].init);
    }
    if (myMeta.tracks[tid].codec == "HEVC"){
      hvccBox.setPayload(myMeta.tracks[tid].init);
    }
    int curPart = 0;
    MP4::SIDX sidxBox;
    sidxBox.setReferenceID(1);
    sidxBox.setTimescale(1000);
    sidxBox.setEarliestPresentationTime(myMeta.tracks[tid].firstms);
    sidxBox.setFirstOffset(0);
    int j = 0;
    for (std::deque<DTSC::Key>::iterator it = myMeta.tracks[tid].keys.begin(); it != myMeta.tracks[tid].keys.end(); it++){
      MP4::sidxReference refItem;
      refItem.referenceType = false;
      refItem.referencedSize = 0;
      for (int i = 0; i < it->getParts(); i++){
        refItem.referencedSize += myMeta.tracks[tid].parts[curPart++].getSize();
      }
      if (myMeta.tracks[tid].codec == "H264"){
        refItem.referencedSize += 14 + avccBox.getSPSLen() + avccBox.getPPSLen();
      }
      if (myMeta.tracks[tid].codec == "HEVC"){
        std::deque<MP4::HVCCArrayEntry> content = hvccBox.getArrays();
        for (std::deque<MP4::HVCCArrayEntry>::iterator it = content.begin(); it != content.end(); it++){
          for (std::deque<std::string>::iterator it2 = it->nalUnits.begin(); it2 != it->nalUnits.end(); it2++){
            refItem.referencedSize += 4 + (*it2).size();
          }
        }
      }
      fragmentSizes[tid][j] = refItem.referencedSize;
      if (it->getLength()){
        refItem.subSegmentDuration = it->getLength();
      }else{
        refItem.subSegmentDuration = myMeta.tracks[tid].lastms - it->getTime();
      }
      refItem.sapStart = false;
      refItem.sapType = 0;
      refItem.sapDeltaTime = 0;
      sidxBox.setReference(refItem, j++);
    }
    return std::string(sidxBox.asBox(),sidxBox.boxedSize());
  }

  std::string OutDashMP4::buildSidx(unsigned int tid, unsigned int keyNum){
    MP4::AVCC avccBox;
    avccBox.setPayload(myMeta.tracks[tid].init);
    int curPart = 0;
    MP4::SIDX sidxBox;
    sidxBox.setReferenceID(1);
    sidxBox.setTimescale(1000);
    sidxBox.setEarliestPresentationTime(myMeta.tracks[tid].keys[keyNum].getTime());
    sidxBox.setFirstOffset(0);
    for (int i = 0; i < keyNum; i++){
      curPart += myMeta.tracks[tid].keys[i].getParts();
    }
    MP4::sidxReference refItem;
    refItem.referenceType = false;
    if (myMeta.tracks[tid].keys[keyNum].getLength()){
      refItem.subSegmentDuration = myMeta.tracks[tid].keys[keyNum].getLength();
    }else{
      refItem.subSegmentDuration = myMeta.tracks[tid].lastms - myMeta.tracks[tid].keys[keyNum].getTime();
    }
    refItem.sapStart = false;
    refItem.sapType = 0;
    refItem.sapDeltaTime = 0;
    sidxBox.setReference(refItem, 0);
    return std::string(sidxBox.asBox(),sidxBox.boxedSize());
  }

  std::string OutDashMP4::buildMoof(unsigned int tid, unsigned int keyNum){
    MP4::MOOF moofBox;
    
    MP4::MFHD mfhdBox;
    mfhdBox.setSequenceNumber(keyNum + 1);
    moofBox.setContent(mfhdBox, 0);
    
    MP4::TRAF trafBox;
    MP4::TFHD tfhdBox;
    if (myMeta.tracks[tid].codec == "H264" || myMeta.tracks[tid].codec == "HEVC"){
      tfhdBox.setTrackID(1);
    }
    if (myMeta.tracks[tid].codec == "AAC"){
      tfhdBox.setFlags(MP4::tfhdSampleFlag);
      tfhdBox.setTrackID(1);
      tfhdBox.setDefaultSampleFlags(MP4::isKeySample);
    }
    trafBox.setContent(tfhdBox, 0);
    
    MP4::TFDT tfdtBox;
    ///\todo Determine index for live
    tfdtBox.setBaseMediaDecodeTime(myMeta.tracks[tid].keys[keyNum].getTime());
    trafBox.setContent(tfdtBox, 1);
    
    int i = 0;
    
    for (int j = 0; j < keyNum; j++){
      i += myMeta.tracks[tid].keys[j].getParts();
    }

    MP4::TRUN trunBox;
    if (myMeta.tracks[tid].codec == "H264"){
      trunBox.setFlags(MP4::trundataOffset | MP4::trunsampleSize | MP4::trunsampleDuration | MP4::trunfirstSampleFlags | MP4::trunsampleOffsets);
      trunBox.setFirstSampleFlags(MP4::isKeySample);
      trunBox.setDataOffset(88 + (12 * myMeta.tracks[tid].keys[keyNum].getParts()) + 8);

      MP4::AVCC avccBox;
      avccBox.setPayload(myMeta.tracks[tid].init);
      for (int j = 0; j < myMeta.tracks[tid].keys[keyNum].getParts(); j++){
        MP4::trunSampleInformation trunEntry;
        if (!j){
          trunEntry.sampleSize = myMeta.tracks[tid].parts[i].getSize() + 14 + avccBox.getSPSLen() + avccBox.getPPSLen();
        }else{
          trunEntry.sampleSize = myMeta.tracks[tid].parts[i].getSize();
        }
        trunEntry.sampleDuration = myMeta.tracks[tid].parts[i].getDuration();
        trunEntry.sampleOffset = myMeta.tracks[tid].parts[i].getOffset();
        trunBox.setSampleInformation(trunEntry, j);
        i++;
      }
    }
    if (myMeta.tracks[tid].codec == "HEVC"){
      trunBox.setFlags(MP4::trundataOffset | MP4::trunsampleSize | MP4::trunsampleDuration | MP4::trunfirstSampleFlags | MP4::trunsampleOffsets);
      trunBox.setFirstSampleFlags(MP4::isKeySample);
      trunBox.setDataOffset(88 + (12 * myMeta.tracks[tid].keys[keyNum].getParts()) + 8);

      MP4::HVCC hvccBox;
      hvccBox.setPayload(myMeta.tracks[tid].init);
      std::deque<MP4::HVCCArrayEntry> content = hvccBox.getArrays();
      for (int j = 0; j < myMeta.tracks[tid].keys[keyNum].getParts(); j++){
        MP4::trunSampleInformation trunEntry;
        trunEntry.sampleSize = myMeta.tracks[tid].parts[i].getSize();
        if (!j){
          for (std::deque<MP4::HVCCArrayEntry>::iterator it = content.begin(); it != content.end(); it++){
            for (std::deque<std::string>::iterator it2 = it->nalUnits.begin(); it2 != it->nalUnits.end(); it2++){
              trunEntry.sampleSize += 4 + (*it2).size();
            }
          }
        }
        trunEntry.sampleDuration = myMeta.tracks[tid].parts[i].getDuration();
        trunEntry.sampleOffset = myMeta.tracks[tid].parts[i].getOffset();
        trunBox.setSampleInformation(trunEntry, j);
        i++;
      }
    }
    if (myMeta.tracks[tid].codec == "AAC"){
      trunBox.setFlags(MP4::trundataOffset | MP4::trunsampleSize | MP4::trunsampleDuration);
      trunBox.setDataOffset(88 + (8 * myMeta.tracks[tid].keys[keyNum].getParts()) + 8);
      for (int j = 0; j < myMeta.tracks[tid].keys[keyNum].getParts(); j++){
        MP4::trunSampleInformation trunEntry;
        trunEntry.sampleSize = myMeta.tracks[tid].parts[i].getSize();
        trunEntry.sampleDuration = myMeta.tracks[tid].parts[i].getDuration();
        trunBox.setSampleInformation(trunEntry, j);
        i++;
      }
    }
    trafBox.setContent(trunBox, 2);
   
    moofBox.setContent(trafBox, 1);
    
    return std::string(moofBox.asBox(), moofBox.boxedSize());
  }
  
  std::string OutDashMP4::buildNalUnit(unsigned int len, const char * data){
    std::stringstream r;
    r << (char)((len >> 24) & 0xFF);
    r << (char)((len >> 16) & 0xFF);
    r << (char)((len >> 8) & 0xFF);
    r << (char)((len) & 0xFF);
    r << std::string(data, len);
    return r.str();
  }
  
  void OutDashMP4::buildMdat(unsigned int tid, unsigned int keyNum){
    MP4::AVCC avccBox;
    avccBox.setPayload(myMeta.tracks[tid].init);
    std::stringstream r;
    int size = fragmentSizes[tid][keyNum] + 8;
    r << (char)((size >> 24) & 0xFF);
    r << (char)((size >> 16) & 0xFF);
    r << (char)((size >> 8) & 0xFF);
    r << (char)((size) & 0xFF);
    r << "mdat";
    H.Chunkify(r.str().data(), r.str().size(), myConn);
    selectedTracks.clear();
    selectedTracks.insert(tid);
    seek(myMeta.tracks[tid].keys[keyNum].getTime());
    std::string init;
    char * data;
    unsigned int dataLen;
    int partNum = 0;
    for (int i = 0; i < keyNum; i++){
      partNum += myMeta.tracks[tid].keys[i].getParts();
    }
    if (myMeta.tracks[tid].codec == "H264"){
      init = buildNalUnit(2, "\011\340");
      H.Chunkify(init, myConn);//09E0
      init = buildNalUnit(avccBox.getSPSLen(), avccBox.getSPS());
      H.Chunkify(init, myConn);
      init = buildNalUnit(avccBox.getPPSLen(), avccBox.getPPS());
      H.Chunkify(init, myConn);
    }
    if (myMeta.tracks[tid].codec == "HEVC"){
      MP4::HVCC hvccBox;
      hvccBox.setPayload(myMeta.tracks[tid].init);
      std::deque<MP4::HVCCArrayEntry> content = hvccBox.getArrays();
      for (int j = 0; j < myMeta.tracks[tid].keys[keyNum].getParts(); j++){
        for (std::deque<MP4::HVCCArrayEntry>::iterator it = content.begin(); it != content.end(); it++){
          for (std::deque<std::string>::iterator it2 = it->nalUnits.begin(); it2 != it->nalUnits.end(); it2++){
            init = buildNalUnit((*it2).size(), (*it2).c_str());
            H.Chunkify(init, myConn);
          }
        }
      }
    }
    for (int i = 0; i < myMeta.tracks[tid].keys[keyNum].getParts(); i++){
      prepareNext();
      currentPacket.getString("data", data, dataLen);
      H.Chunkify(data, dataLen, myConn);
    }
    return;
  }
    
  std::string OutDashMP4::buildManifest(){
    initialize();
    int lastTime = 0;
    int lastVidTime = 0;
    int vidKeys = 0;
    int vidInitTrack = 0;
    int lastAudTime = 0;
    int audKeys = 0;
    int audInitTrack = 0;
    for (std::map<unsigned int, DTSC::Track>::iterator it = myMeta.tracks.begin(); it != myMeta.tracks.end(); it ++){
      if (it->second.lastms > lastTime){
        lastTime = it->second.lastms;
      }
      if (it->second.codec == "H264" && it->second.lastms > lastVidTime){
        lastVidTime = it->second.lastms;
        vidKeys = it->second.keys.size();
        vidInitTrack = it->first;
      }
      if (it->second.codec == "HEVC" && it->second.lastms > lastVidTime){
        lastVidTime = it->second.lastms;
        vidKeys = it->second.keys.size();
        vidInitTrack = it->first;
      }
      if (it->second.codec == "AAC" && it->second.lastms > lastAudTime){
        lastAudTime = it->second.lastms;
        audKeys = it->second.keys.size();
        audInitTrack = it->first;
      }
    }
    std::stringstream r;
    r << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    r << "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"urn:mpeg:dash:schema:mpd:2011\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" type=\"static\" mediaPresentationDuration=\"" << makeTime(lastTime) << "\" minBufferTime=\"PT1.5S\" >" << std::endl;
    r << "  <ProgramInformation><Title>" << streamName << "</Title></ProgramInformation>" << std::endl;
    r << "  <Period start=\"PT0S\">" << std::endl;
    if (vidInitTrack){
      r << "    <AdaptationSet id=\"0\" mimeType=\"video/mp4\" width=\"" << myMeta.tracks[vidInitTrack].width << "\" height=\"" << myMeta.tracks[vidInitTrack].height << "\" frameRate=\"" << myMeta.tracks[vidInitTrack].fpks / 1000 << "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">" << std::endl;
      r << "      <SegmentTemplate timescale=\"1000\" media=\"chunk_$RepresentationID$_$Time$.m4s\" initialization=\"chunk_$RepresentationID$_init.m4s\">" << std::endl;
      r << "        <SegmentTimeline>" << std::endl;
      for (int i = 0; i < myMeta.tracks[vidInitTrack].keys.size() - 1; i++){
        r << "          <S " << (i == 0 ? "t=\"0\" " : "") << "d=\"" << myMeta.tracks[vidInitTrack].keys[i].getLength() << "\" />" << std::endl;
      }
      int lastDur = myMeta.tracks[vidInitTrack].lastms - myMeta.tracks[vidInitTrack].keys.rbegin()->getTime();
      r << "          <S d=\"" << lastDur << "\" />" << std::endl;
      r << "        </SegmentTimeline>" << std::endl;
      r << "      </SegmentTemplate>" << std::endl;
      for (std::map<unsigned int, DTSC::Track>::iterator it = myMeta.tracks.begin(); it != myMeta.tracks.end(); it++){
        if (it->second.codec == "H264"){
          MP4::AVCC avccBox;
          avccBox.setPayload(it->second.init);
          r << "      <Representation ";
          r << "id=\"" << it->first << "\" ";
          r << "codecs=\"avc1.";
          r << std::hex << std::setw(2) << std::setfill('0') << (int)avccBox.getSPS()[0] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)avccBox.getSPS()[1] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)avccBox.getSPS()[2] << std::dec;
          r << "\" ";
          r << "bandwidth=\"" << it->second.bps << "\" ";
          r << "/>" << std::endl;
        }
        if (it->second.codec == "HEVC"){
          r << "      <Representation ";
          r << "id=\"" << it->first << "\" ";
          r << "codecs=\"hev1.";
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[1] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[6] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[7] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[8] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[9] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[10] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[11] << std::dec;
          r << std::hex << std::setw(2) << std::setfill('0') << (int)it->second.init[12] << std::dec;
          r << "\" ";
          r << "bandwidth=\"" << it->second.bps << "\" ";
          r << "/>" << std::endl;
        }
      }
      r << "    </AdaptationSet>" << std::endl;
    }
    if (audInitTrack){
      r << "    <AdaptationSet id=\"1\" mimeType=\"audio/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\" >" << std::endl;
      r << "      <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>" << std::endl;
      r << "      <SegmentTemplate timescale=\"1000\" media=\"chunk_$RepresentationID$_$Time$.m4s\" initialization=\"chunk_$RepresentationID$_init.m4s\">" << std::endl;

      r << "        <SegmentTimeline>" << std::endl;
      for (int i = 0; i < myMeta.tracks[audInitTrack].keys.size() - 1; i++){
        r << "          <S " << (i == 0 ? "t=\"0\" " : "") << "d=\"" << myMeta.tracks[audInitTrack].keys[i].getLength() << "\" />" << std::endl;
      }
      int lastDur = myMeta.tracks[audInitTrack].lastms - myMeta.tracks[audInitTrack].keys.rbegin()->getTime();
      r << "          <S d=\"" << lastDur << "\" />" << std::endl;
      r << "        </SegmentTimeline>" << std::endl;
      r << "      </SegmentTemplate>" << std::endl;
 
      for (std::map<unsigned int, DTSC::Track>::iterator it = myMeta.tracks.begin(); it != myMeta.tracks.end(); it++){
        if (it->second.codec == "AAC"){
          r << "      <Representation ";
          r << "id=\"" << it->first << "\" ";
          r << "codecs=\"mp4a.40.2\" ";
          r << "audioSamplingRate=\"" << it->second.rate << "\" ";
          r << "bandwidth=\"" << it->second.bps << "\">" << std::endl;
          r << "        <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"" << it->second.channels << "\" />" << std::endl;
          r << "      </Representation>" << std::endl;
        }
      }
      r << "    </AdaptationSet>" << std::endl;
    }
    r << "  </Period>" << std::endl;
    r << "</MPD>" << std::endl;

    return r.str();
  }
  
  void OutDashMP4::init(Util::Config * cfg){
    HTTPOutput::init(cfg);
    capa["name"] = "DASHMP4";
    capa["desc"] = "Enables HTTP protocol progressive streaming.";
    capa["url_rel"] = "/dash/$/index.mpd";
    capa["url_prefix"] = "/dash/$/";
    capa["socket"] = "http_dash_mp4";
    capa["codecs"][0u][0u].append("H264");
    capa["codecs"][0u][0u].append("HEVC");
    capa["codecs"][0u][1u].append("AAC");
    capa["codecs"][0u][1u].append("AC3");
    capa["methods"][0u]["handler"] = "http";
    capa["methods"][0u]["type"] = "dash/video/mp4";
    capa["methods"][0u]["priority"] = 8ll;
    capa["methods"][0u]["nolive"] = 1;
  }
  
  /// Parses a "Range: " header, setting byteStart, byteEnd and seekPoint using data from metadata and tracks to do
  /// the calculations.
  /// On error, byteEnd is set to zero.
  void OutDashMP4::parseRange(std::string header, long long & byteStart, long long & byteEnd){
    int firstPos = header.find("=") + 1;
    byteStart = atoll(header.substr(firstPos, header.find("-", firstPos)).c_str());
    byteEnd = atoll(header.substr(header.find("-", firstPos) + 1).c_str());
    
    DEBUG_MSG(DLVL_DEVEL, "Range request: %lli-%lli (%s)", byteStart, byteEnd, header.c_str());
  }
  
  int OutDashMP4::getKeyFromRange(unsigned int tid, long long int byteStart){
    unsigned long long int currOffset = 0;
    for (int i = 0; i < myMeta.tracks[tid].keys.size(); i++){
      if (byteStart == currOffset){
        return i;
      }
      if (byteStart < currOffset && i > 0){
        return i - 1;
      }
      DEBUG_MSG(DLVL_DEVEL, "%lld > %llu", byteStart, currOffset);
    }
    return -1;
  }

  void OutDashMP4::initialize(){
    HTTPOutput::initialize();
    for (std::map<unsigned int,DTSC::Track>::iterator it = myMeta.tracks.begin(); it != myMeta.tracks.end(); it++){
      if (!moovBoxes.count(it->first)){
        moovBoxes[it->first] = buildMoov(it->first);
        buildSidx(it->first);
      }
    }
  }
  
  void OutDashMP4::onHTTP(){
    initialize();
    if (H.method == "OPTIONS"){
      H.Clean();
      H.SetHeader("Content-Type", "application/octet-stream");
      H.SetHeader("Cache-Control", "no-cache");
      H.SetHeader("MistMultiplex", "No");
      
      H.SetHeader("Access-Control-Allow-Origin", "*");
      H.SetHeader("Access-Control-Allow-Methods", "GET, POST");
      H.SetHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With");
      H.SetHeader("Access-Control-Allow-Credentials", "true");
      H.SetBody("");
      H.SendResponse("200", "OK", myConn);
      H.Clean();
      return;
    }
    if (H.url.find(".mpd") != std::string::npos){
      H.Clean();
      H.SetHeader("Content-Type", "application/xml");
      H.SetHeader("Cache-Control", "no-cache");
      H.SetHeader("MistMultiplex", "No");
      
      H.SetHeader("Access-Control-Allow-Origin", "*");
      H.SetHeader("Access-Control-Allow-Methods", "GET, POST");
      H.SetHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With");
      H.SetHeader("Access-Control-Allow-Credentials", "true");

      H.SetBody(buildManifest());
      H.SendResponse("200", "OK", myConn);
      DEVEL_MSG("Manifest sent");
    }else{
      long long int bench = Util::getMS();
      int pos = H.url.find("chunk_") + 6;//put our marker just after the _ beyond chunk
      int tid = atoi(H.url.substr(pos).c_str());
      DEBUG_MSG(DLVL_DEVEL, "Track %d requested", tid);

      H.Clean();
      H.SetHeader("Content-Type", "video/mp4");
      H.SetHeader("Cache-Control", "no-cache");
      H.SetHeader("MistMultiplex", "No");
      
      H.SetHeader("Access-Control-Allow-Origin", "*");
      H.SetHeader("Access-Control-Allow-Methods", "GET, POST");
      H.SetHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With");
      H.SetHeader("Access-Control-Allow-Credentials", "true");
      H.StartResponse(H, myConn);

      if (H.url.find("init.m4s") != std::string::npos){
        DEBUG_MSG(DLVL_DEVEL, "Handling init");
        buildFtyp(tid);
        H.Chunkify(moovBoxes[tid], myConn);
      }else{
        pos = H.url.find("_", pos + 1) + 1;
        int keyId = atoi(H.url.substr(pos).c_str());
        DEBUG_MSG(DLVL_DEVEL, "Searching for time %d", keyId);
        unsigned int keyNum = myMeta.tracks[tid].timeToKeynum(keyId);
        INFO_MSG("Detected key %d:%d for time %d", tid, keyNum, keyId);
        buildStyp(tid);
        std::string tmp = buildSidx(tid, keyNum);
        H.Chunkify(tmp, myConn);
        tmp = buildMoof(tid, keyNum);
        H.Chunkify(tmp, myConn);
        buildMdat(tid, keyNum);
      }
      H.Chunkify("", 0, myConn);
      H.Clean();
      INFO_MSG("Done handling request, took %lld ms", Util::getMS() - bench);
      return;
    }
    H.Clean();
    parseData = false;
    wantRequest = true;
  }
  
  bool OutDashMP4::onFinish(){return false;}
  void OutDashMP4::sendNext(){}
  void OutDashMP4::sendHeader(){}
}