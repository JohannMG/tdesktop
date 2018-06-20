/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

namespace Export {
namespace Data {
struct PersonalInfo;
struct UserpicsInfo;
struct UserpicsSlice;
struct ContactsList;
struct SessionsList;
struct DialogsInfo;
struct DialogInfo;
struct MessagesSlice;
} // namespace Data

struct Settings;

namespace Output {

struct Result;
class Stats;

enum class Format {
	Text,
	Yaml,
	Html,
	Json,
};

class AbstractWriter {
public:
	[[nodiscard]] virtual Result start(
		const Settings &settings,
		Stats *stats) = 0;

	[[nodiscard]] virtual Result writePersonal(
		const Data::PersonalInfo &data) = 0;

	[[nodiscard]] virtual Result writeUserpicsStart(
		const Data::UserpicsInfo &data) = 0;
	[[nodiscard]] virtual Result writeUserpicsSlice(
		const Data::UserpicsSlice &data) = 0;
	[[nodiscard]] virtual Result writeUserpicsEnd() = 0;

	[[nodiscard]] virtual Result writeContactsList(
		const Data::ContactsList &data) = 0;

	[[nodiscard]] virtual Result writeSessionsList(
		const Data::SessionsList &data) = 0;

	[[nodiscard]] virtual Result writeDialogsStart(
		const Data::DialogsInfo &data) = 0;
	[[nodiscard]] virtual Result writeDialogStart(
		const Data::DialogInfo &data) = 0;
	[[nodiscard]] virtual Result writeDialogSlice(
		const Data::MessagesSlice &data) = 0;
	[[nodiscard]] virtual Result writeDialogEnd() = 0;
	[[nodiscard]] virtual Result writeDialogsEnd() = 0;

	[[nodiscard]] virtual Result writeLeftChannelsStart(
		const Data::DialogsInfo &data) = 0;
	[[nodiscard]] virtual Result writeLeftChannelStart(
		const Data::DialogInfo &data) = 0;
	[[nodiscard]] virtual Result writeLeftChannelSlice(
		const Data::MessagesSlice &data) = 0;
	[[nodiscard]] virtual Result writeLeftChannelEnd() = 0;
	[[nodiscard]] virtual Result writeLeftChannelsEnd() = 0;

	[[nodiscard]] virtual Result finish() = 0;

	[[nodiscard]] virtual QString mainFilePath() = 0;

	virtual ~AbstractWriter() = default;

};

std::unique_ptr<AbstractWriter> CreateWriter(Format format);

} // namespace Output
} // namespace Export