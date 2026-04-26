#include "Score.h"
#include "File.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"
#include "IO.h"
#include "Constants.h"
#include <unordered_set>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <fstream>

using namespace IO;

namespace MikuMikuWorld
{
	using json = nlohmann::ordered_json;

	enum NoteFlags
	{
		NOTE_CRITICAL = 1 << 0,
		NOTE_FRICTION = 1 << 1
	};

	enum HoldFlags
	{
		HOLD_START_HIDDEN	= 1 << 0,
		HOLD_END_HIDDEN		= 1 << 1,
		HOLD_GUIDE			= 1 << 2
	};

	Score::Score()
	{
		metadata.title = "";
		metadata.author = "";
		metadata.artist = "";
		metadata.musicOffset = 0;

		tempoChanges.push_back(Tempo());
		timeSignatures[0] = { 0, 4, 4 };

		fever.startTick = fever.endTick = -1;
	}

	Note readNote(NoteType type, BinaryReader* reader)
	{
		Note note(type);
		note.tick = reader->readInt32();
		note.lane = reader->readInt32();
		note.width = reader->readInt32();

		if (!note.hasEase())
			note.flick = (FlickType)reader->readInt32();

		unsigned int flags = reader->readInt32();
		note.critical = (bool)(flags & NOTE_CRITICAL);
		note.friction = (bool)(flags & NOTE_FRICTION);
		return note;
	}

	void writeNote(const Note& note, BinaryWriter* writer)
	{
		writer->writeInt32(note.tick);
		writer->writeInt32(note.lane);
		writer->writeInt32(note.width);

		if (!note.hasEase())
			writer->writeInt32((int)note.flick);
		
		unsigned int flags{};
		if (note.critical) flags |= NOTE_CRITICAL;
		if (note.friction) flags |= NOTE_FRICTION;
		writer->writeInt32(flags);
	}

	ScoreMetadata readMetadata(BinaryReader* reader, int version)
	{
		ScoreMetadata metadata;
		metadata.title = reader->readString();
		metadata.author = reader->readString();
		metadata.artist = reader->readString();
		metadata.genre = reader->readString();
		try
		{
			metadata.level = std::stoi(reader->readString());
		}
		catch (const std::exception&)
		{
			metadata.level = 0;
		}
		metadata.movie_name = reader->readString();

		try
		{
			metadata.movie_offset = std::stof(reader->readString());
		}
		catch (const std::exception&)
		{
			metadata.movie_offset = 0;
		}
		
		//文字列からboolへ
		std::string lowerStr = reader->readString();
		std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
			[](unsigned char c) { return std::tolower(c); });
		bool islong = (lowerStr == "true" || lowerStr == "1");

		metadata.islong = islong;
		metadata.musicFile = reader->readString();
		metadata.musicOffset = reader->readSingle();

		if (version > 1)
			metadata.jacketFile = reader->readString();

		return metadata;
	}

	void writeMetadata(const ScoreMetadata& metadata, BinaryWriter* writer)
	{
		writer->writeString(metadata.title);
		writer->writeString(metadata.author);
		writer->writeString(metadata.artist);
		writer->writeString(metadata.genre);
		writer->writeString(std::to_string(metadata.level));
		writer->writeString(metadata.movie_name);
		writer->writeString(std::to_string(metadata.movie_offset));
		writer->writeString(std::to_string(metadata.islong));
		writer->writeString(metadata.musicFile);
		writer->writeSingle(metadata.musicOffset);
		writer->writeString(metadata.jacketFile);
	}

	void readScoreEvents(Score& score, int version, BinaryReader* reader)
	{
		// time signature
		int timeSignatureCount = reader->readInt32();
		if (timeSignatureCount)
			score.timeSignatures.clear();

		for (int i = 0; i < timeSignatureCount; ++i)
		{
			int measure = reader->readInt32();
			int numerator = reader->readInt32();
			int denominator = reader->readInt32();
			score.timeSignatures[measure] = { measure, numerator, denominator };
		}

		// bpm
		int tempoCount = reader->readInt32();
		if (tempoCount)
			score.tempoChanges.clear();

		for (int i = 0; i < tempoCount; ++i)
		{
			int tick = reader->readInt32();
			float bpm = reader->readSingle();
			score.tempoChanges.push_back({ tick, bpm });
		}

		// hi-speed
		if (version > 2)
		{
			int hiSpeedCount = reader->readInt32();
			for (int i = 0; i < hiSpeedCount; ++i)
			{
				int tick = reader->readInt32();
				float speed = reader->readSingle();

				score.hiSpeedChanges.push_back({ tick, speed });
			}
		}

		// skills and fever
		if (version > 1)
		{
			int skillCount = reader->readInt32();
			for (int i = 0; i < skillCount; ++i)
			{
				int tick = reader->readInt32();
				score.skills.push_back({ nextSkillID++, tick });
			}

			score.fever.startTick = reader->readInt32();
			score.fever.endTick = reader->readInt32();
		}

		// sections
		if (version > 4)
		{
			int sectionCount = reader->readInt32();
			for (int i = 0; i < sectionCount; ++i)
			{
				int measure = reader->readInt32();
				std::string name = reader->readString();
				score.sections[measure] = name;
			}
		}
	}

	void writeScoreEvents(const Score& score, BinaryWriter* writer)
	{
		writer->writeInt32(score.timeSignatures.size());
		for (const auto& [_, timeSignature] : score.timeSignatures)
		{
			writer->writeInt32(timeSignature.measure);
			writer->writeInt32(timeSignature.numerator);
			writer->writeInt32(timeSignature.denominator);
		}

		writer->writeInt32(score.tempoChanges.size());
		for (const auto& tempo : score.tempoChanges)
		{
			writer->writeInt32(tempo.tick);
			writer->writeSingle(tempo.bpm);
		}

		writer->writeInt32(score.hiSpeedChanges.size());
		for (const auto& hiSpeed : score.hiSpeedChanges)
		{
			writer->writeInt32(hiSpeed.tick);
			writer->writeSingle(hiSpeed.speed);
		}

		writer->writeInt32(score.skills.size());
		for (const auto& skill : score.skills)
		{
			writer->writeInt32(skill.tick);
		}

		writer->writeInt32(score.fever.startTick);
		writer->writeInt32(score.fever.endTick);

		// sections
		writer->writeInt32(score.sections.size());
		for (const auto& [measure, name] : score.sections)
		{
			writer->writeInt32(measure);
			writer->writeString(name);
		}
	}

	Score deserializeScore(const std::string& filename)
	{
		Score score;
		BinaryReader reader(filename);
		if (!reader.isStreamValid())
			return score;

		std::string signature = reader.readString();
		if (signature != "MMWS")
			throw std::runtime_error("Not a MMWS file.");

		int version = reader.readInt32();

		uint32_t metadataAddress{};
		uint32_t eventsAddress{};
		uint32_t tapsAddress{};
		uint32_t holdsAddress{};
		if (version > 2)
		{
			metadataAddress = reader.readInt32();
			eventsAddress = reader.readInt32();
			tapsAddress = reader.readInt32();
			holdsAddress = reader.readInt32();

			reader.seek(metadataAddress);
		}

		score.metadata = readMetadata(&reader, version);

		if (version > 2)
			reader.seek(eventsAddress);

		readScoreEvents(score, version, &reader);

		if (version > 2)
			reader.seek(tapsAddress);

		int noteCount = reader.readInt32();
		score.notes.reserve(noteCount);
		for (int i = 0; i < noteCount; ++i)
		{
			Note note = readNote(NoteType::Tap, &reader);
			note.ID = nextID++;
			score.notes[note.ID] = note;
		}

		if (version > 2)
			reader.seek(holdsAddress);

		int holdCount = reader.readInt32();
		score.holdNotes.reserve(holdCount);
		for (int i = 0; i < holdCount; ++i)
		{
			HoldNote hold;

			unsigned int flags{};
			if (version > 3)
				flags = reader.readInt32();

			if (flags & HOLD_START_HIDDEN)
				hold.startType = HoldNoteType::Hidden;

			if (flags & HOLD_END_HIDDEN)
				hold.endType = HoldNoteType::Hidden;

			if (flags & HOLD_GUIDE)
				hold.startType = hold.endType = HoldNoteType::Guide;

			Note start = readNote(NoteType::Hold, &reader);
			start.ID = nextID++;
			hold.start.ease = (EaseType)reader.readInt32();
			hold.start.ID = start.ID;
			score.notes[start.ID] = start;

			int stepCount = reader.readInt32();
			hold.steps.reserve(stepCount);
			for (int i = 0; i < stepCount; ++i)
			{
				Note mid = readNote(NoteType::HoldMid, &reader);
				mid.ID = nextID++;
				mid.parentID = start.ID;
				score.notes[mid.ID] = mid;

				HoldStep step{};
				step.type = (HoldStepType)reader.readInt32();
				step.ease = (EaseType)reader.readInt32();
				step.ID = mid.ID;
				hold.steps.push_back(step);
			}

			Note end = readNote(NoteType::HoldEnd, &reader);
			end.ID = nextID++;
			end.parentID = start.ID;
			score.notes[end.ID] = end;
			
			hold.end = end.ID;
			score.holdNotes[start.ID] = hold;
		}

		reader.close();
		return score;
	}

	void serializeScore(const Score& score, const std::string& filename)
	{
		BinaryWriter writer(filename);
		if (!writer.isStreamValid())
			return;

		// signature
		writer.writeString("MMWS");

		// verison
		writer.writeInt32(5);

		// offsets address in order: metadata -> events -> taps -> holds
		uint32_t offsetsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t) * 4);

		uint32_t metadataAddress = writer.getStreamPosition();
		writeMetadata(score.metadata, &writer);

		uint32_t eventsAddress = writer.getStreamPosition();
		writeScoreEvents(score, &writer);

		uint32_t tapsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t));

		int noteCount = 0;
		for (const auto&[id, note] : score.notes)
		{
			if (note.getType() != NoteType::Tap)
				continue;

			writeNote(note, &writer);
			++noteCount;
		}
		
		uint32_t holdsAddress = writer.getStreamPosition();
		
		// write taps count
		writer.seek(tapsAddress);
		writer.writeInt32(noteCount);
		writer.seek(holdsAddress);
		
		writer.writeInt32(score.holdNotes.size());
		for (const auto&[id, hold] : score.holdNotes)
		{	
			unsigned int flags{};
			if (hold.startType == HoldNoteType::Guide) flags	|=	HOLD_GUIDE;
			if (hold.startType == HoldNoteType::Hidden) flags	|=	HOLD_START_HIDDEN;
			if (hold.endType == HoldNoteType::Hidden) flags		|=	HOLD_END_HIDDEN;
			writer.writeInt32(flags);

			// note data
			const Note& start = score.notes.at(hold.start.ID);
			writeNote(start, &writer);
			writer.writeInt32((int)hold.start.ease);

			// steps
			int stepCount = hold.steps.size();
			writer.writeInt32(stepCount);
			for (const auto& step : hold.steps)
			{
				const Note& mid = score.notes.at(step.ID);
				writeNote(mid, &writer);
				writer.writeInt32((int)step.type);
				writer.writeInt32((int)step.ease);
			}

			// end
			const Note& end = score.notes.at(hold.end);
			writeNote(end, &writer);
		}

		// write offset addresses
		writer.seek(offsetsAddress);
		writer.writeInt32(metadataAddress);
		writer.writeInt32(eventsAddress);
		writer.writeInt32(tapsAddress);
		writer.writeInt32(holdsAddress);

		writer.flush();
		writer.close();
	}
	
	void serializeScoreToJson(const Score& score, const std::string& filename)
	{
        json data;

        const ScoreMetadata& metadata = score.metadata;
        json metadataJson;
        metadataJson["title"] = metadata.title;
        metadataJson["artist"] = metadata.artist;
        metadataJson["author"] = metadata.author;
        metadataJson["genre"] = metadata.genre;
        metadataJson["level"] = metadata.level;
        metadataJson["movie_name"] = metadata.movie_name;
        metadataJson["movie_offset"] = metadata.movie_offset;
        metadataJson["islong"] = metadata.islong;
        metadataJson["musicFile"] = metadata.musicFile;
        metadataJson["musicOffset"] = metadata.musicOffset;
        metadataJson["jacketFile"] = metadata.jacketFile;
        data["metadata"] = metadataJson;

        json timeSignatures = json::array();
        for (const auto&[_, timeSignature] : score.timeSignatures)
        {
                json tsJson;
                tsJson["measure"] = timeSignature.measure;
                tsJson["numerator"] = timeSignature.numerator;
                tsJson["denominator"] = timeSignature.denominator;
                timeSignatures.push_back(tsJson);
        }
        data["timeSignatures"] = timeSignatures;

        json tempoChanges = json::array();
        for (const auto& tempo : score.tempoChanges)
        {
                json tempoJson;
                tempoJson["tick"] = tempo.tick;
                tempoJson["bpm"] = tempo.bpm;
                tempoChanges.push_back(tempoJson);
        }
        data["tempoChanges"] = tempoChanges;

        json hiSpeedChanges = json::array();
        for (const auto& hiSpeed : score.hiSpeedChanges)
        {
                json hiSpeedJson;
                hiSpeedJson["tick"] = hiSpeed.tick;
                hiSpeedJson["speed"] = hiSpeed.speed;
                hiSpeedChanges.push_back(hiSpeedJson);
        }
        data["hiSpeedChanges"] = hiSpeedChanges;

        json skills = json::array();
        for (const auto& skill : score.skills)
        {
                json skillJson;
                skillJson["id"] = skill.ID;
                skillJson["tick"] = skill.tick;
                skills.push_back(skillJson);
        }
        data["skills"] = skills;

        json feverJson;
        feverJson["startTick"] = score.fever.startTick;
        feverJson["endTick"] = score.fever.endTick;
        data["fever"] = feverJson;

        json sections = json::array();
        for (const auto& [measure, name] : score.sections)
        {
                json sectionJson;
                sectionJson["measure"] = measure;
                sectionJson["name"] = name;
                sections.push_back(sectionJson);
        }
        data["sections"] = sections;

        auto noteToJson = [](const Note& note)
        {
                json noteJson;
                noteJson["id"] = note.ID;
                noteJson["tick"] = note.tick;
                noteJson["lane"] = note.lane;
                noteJson["width"] = note.width;
                noteJson["critical"] = note.critical;
                noteJson["friction"] = note.friction;
                noteJson["noteType"] = (int)note.getType();
                if (!note.hasEase())
                {
                        noteJson["flick"] = flickTypes[(int)note.flick];
                }
                if (note.parentID != -1)
                {
                        noteJson["parentId"] = note.parentID;
                }
                return noteJson;
        };

        json notesArray = json::array();
        std::vector<const Note*> taps;
        taps.reserve(score.notes.size());
        for (const auto&[_, note] : score.notes)
        {
                if (note.getType() == NoteType::Tap)
                {
                        taps.push_back(&note);
                }
        }
        std::sort(taps.begin(), taps.end(), [](const Note* lhs, const Note* rhs)
        {
                if (lhs->tick != rhs->tick)
                        return lhs->tick < rhs->tick;
                if (lhs->lane != rhs->lane)
                        return lhs->lane < rhs->lane;
                return lhs->ID < rhs->ID;
        });
        for (const Note* note : taps)
        {
                notesArray.push_back(noteToJson(*note));
        }
        data["notes"] = notesArray;

        json holdsArray = json::array();
        std::vector<const HoldNote*> holdList;
        holdList.reserve(score.holdNotes.size());
        for (const auto&[_, hold] : score.holdNotes)
        {
                holdList.push_back(&hold);
        }
        std::sort(holdList.begin(), holdList.end(), [&score](const HoldNote* lhs, const HoldNote* rhs)
        {
                const Note& lhsStart = score.notes.at(lhs->start.ID);
                const Note& rhsStart = score.notes.at(rhs->start.ID);
                if (lhsStart.tick != rhsStart.tick)
                        return lhsStart.tick < rhsStart.tick;
                if (lhsStart.lane != rhsStart.lane)
                        return lhsStart.lane < rhsStart.lane;
                return lhsStart.ID < rhsStart.ID;
        });
        for (const HoldNote* hold : holdList)
        {
                json holdJson;
                const Note& start = score.notes.at(hold->start.ID);
                json startJson = noteToJson(start);
                startJson["ease"] = easeTypes[(int)hold->start.ease];
                startJson["holdType"] = holdTypes[(int)hold->startType];
                holdJson["start"] = startJson;

                json stepsJson = json::array();
                for (const auto& step : hold->steps)
                {
                        const Note& mid = score.notes.at(step.ID);
                        json stepJson = noteToJson(mid);
                        stepJson["stepType"] = stepTypes[(int)step.type];
                        stepJson["ease"] = easeTypes[(int)step.ease];
                        stepsJson.push_back(stepJson);
                }
                holdJson["steps"] = stepsJson;

                const Note& end = score.notes.at(hold->end);
                json endJson = noteToJson(end);
                endJson["holdType"] = holdTypes[(int)hold->endType];
                holdJson["end"] = endJson;

                holdsArray.push_back(holdJson);
        }
        data["holds"] = holdsArray;

		// マルチバイト文字対応のため、ワイド文字列に変換してから開く
		std::ofstream output(IO::mbToWideStr(filename));
		if (!output.is_open())
			throw std::runtime_error("Failed to open JSON file for writing."); // 例外を投げてエラーを通知

        output << data.dump(4);
	}
}
