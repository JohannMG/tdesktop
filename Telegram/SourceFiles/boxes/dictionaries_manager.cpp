/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/dictionaries_manager.h"

#ifndef TDESKTOP_DISABLE_SPELLCHECK

#include "chat_helpers/spellchecker_common.h"
#include "core/application.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "mtproto/dedicated_file_loader.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/effects/animations.h"

namespace Ui {
namespace {

using Dictionaries = std::vector<int>;
using namespace Storage::CloudBlob;

using Loading = MTP::DedicatedLoader::Progress;
using DictState = BlobState;

class Loader : public BlobLoader {
public:
	Loader(
		QObject *parent,
		int id,
		MTP::DedicatedLoader::Location location,
		const QString &folder,
		int size);

	void destroy() override;
	void unpack(const QString &path) override;

};

class Inner : public Ui::RpWidget {
public:
	Inner(QWidget *parent, Dictionaries enabledDictionaries);

	Dictionaries enabledRows() const;

private:
	void setupContent(Dictionaries enabledDictionaries);

	Dictionaries _enabledRows;

};

base::unique_qptr<Loader> GlobalLoader;
rpl::event_stream<Loader*> GlobalLoaderValues;

void SetGlobalLoader(base::unique_qptr<Loader> loader) {
	GlobalLoader = std::move(loader);
	GlobalLoaderValues.fire(GlobalLoader.get());
}

inline auto DictExists(int langId) {
	return Spellchecker::DictionaryExists(langId);
}

DictState ComputeState(int id, bool enabled) {
	const auto result = enabled ? DictState(Active()) : DictState(Ready());
	if (DictExists(id)) {
		return result;
	}
	return Available{ Spellchecker::GetDownloadSize(id) };
}

QString StateDescription(const DictState &state) {
	return StateDescription(
		state,
		tr::lng_settings_manage_enabled_dictionary);
}

Loader::Loader(
	QObject *parent,
	int id,
	MTP::DedicatedLoader::Location location,
	const QString &folder,
	int size) : BlobLoader(parent, id, location, folder, size) {
}

void Loader::unpack(const QString &path) {
	const auto weak = Ui::MakeWeak(this);
	crl::async([=] {
		if (Spellchecker::UnpackDictionary(path, id())) {
			QFile(path).remove();
			crl::on_main(weak, [=] {
				destroy();
			});
		} else {
			crl::on_main(weak, [=] {
				fail();
			});
		}
	});
}

void Loader::destroy() {
	Expects(GlobalLoader == this);

	SetGlobalLoader(nullptr);
}

Inner::Inner(
	QWidget *parent,
	Dictionaries enabledDictionaries) : RpWidget(parent) {
	setupContent(std::move(enabledDictionaries));
}

Dictionaries Inner::enabledRows() const {
	return _enabledRows;
}

auto AddButtonWithLoader(
		not_null<Ui::VerticalLayout*> content,
		const Spellchecker::Dict &dict,
		bool buttonEnabled) {
	const auto id = dict.id;

	const auto button = content->add(
		object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
			content,
			object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(dict.name),
				st::dictionariesSectionButton
			)
		)
	)->entity();

	const auto buttonState = button->lifetime()
		.make_state<rpl::variable<DictState>>();

	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		button,
		buttonState->value() | rpl::map(StateDescription),
		st::settingsUpdateState);
	label->setAttribute(Qt::WA_TransparentForMouseEvents);

	rpl::combine(
		button->widthValue(),
		label->widthValue()
	) | rpl::start_with_next([=] {
		label->moveToLeft(
			st::settingsUpdateStatePosition.x(),
			st::settingsUpdateStatePosition.y());
	}, label->lifetime());

	buttonState->value(
	) | rpl::start_with_next([=](const DictState &state) {
		const auto isToggledSet = state.is<Active>();
		const auto toggled = isToggledSet ? 1. : 0.;
		const auto over = !button->isDisabled()
			&& (button->isDown() || button->isOver());

		if (toggled == 0. && !over) {
			label->setTextColorOverride(std::nullopt);
		} else {
			label->setTextColorOverride(anim::color(
				over ? st::contactsStatusFgOver : st::contactsStatusFg,
				st::contactsStatusFgOnline,
				toggled));
		}
	}, label->lifetime());

	button->toggleOn(
		rpl::single(
			buttonEnabled
		) | rpl::then(
			buttonState->value(
			) | rpl::filter([](const DictState &state) {
				return state.is<Failed>();
			}) | rpl::map([](const auto &state) {
				return false;
			})
		)
	);

	*buttonState = GlobalLoaderValues.events_starting_with(
		GlobalLoader.get()
	) | rpl::map([=](Loader *loader) {
		return (loader && loader->id() == id)
			? loader->state()
			: rpl::single(
				buttonEnabled
			) | rpl::then(
				button->toggledValue()
			) | rpl::map([=](auto enabled) {
				return ComputeState(id, enabled);
			});
	}) | rpl::flatten_latest(
	) | rpl::filter([=](const DictState &state) {
		return !buttonState->current().is<Failed>() || !state.is<Available>();
	});

	button->toggledValue(
	) | rpl::start_with_next([=](bool toggled) {
		const auto &state = buttonState->current();
		if (toggled && (state.is<Available>() || state.is<Failed>())) {
			SetGlobalLoader(base::make_unique_q<Loader>(
				App::main(),
				id,
				Spellchecker::GetDownloadLocation(id),
				Spellchecker::DictPathByLangId(id),
				Spellchecker::GetDownloadSize(id)));
		} else if (!toggled && state.is<Loading>()) {
			if (GlobalLoader && GlobalLoader->id() == id) {
				GlobalLoader->destroy();
			}
		}
	}, button->lifetime());

	return button;
}

void Inner::setupContent(Dictionaries enabledDictionaries) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	for (const auto &dict : Spellchecker::Dictionaries()) {
		const auto id = dict.id;
		const auto row = AddButtonWithLoader(
			content,
			dict,
			ranges::contains(enabledDictionaries, id));
		row->toggledValue(
		) | rpl::start_with_next([=](auto enabled) {
			if (enabled) {
				_enabledRows.push_back(id);
			} else {
				auto &rows = _enabledRows;
				rows.erase(ranges::remove(rows, id), end(rows));
			}
		}, row->lifetime());
	}

	content->resizeToWidth(st::boxWidth);
	Ui::ResizeFitChild(this, content);
}

} // namespace

ManageDictionariesBox::ManageDictionariesBox(
	QWidget*,
	not_null<Main::Session*> session)
: _session(session) {
}

void ManageDictionariesBox::prepare() {
	const auto inner = setInnerWidget(object_ptr<Inner>(
		this,
		_session->settings().dictionariesEnabled()));

	setTitle(tr::lng_settings_manage_dictionaries());

	addButton(tr::lng_settings_save(), [=] {
		auto enabledRows = inner->enabledRows();
		_session->settings().setDictionariesEnabled(
			enabledRows | ranges::views::filter(
				DictExists
			) | ranges::to_vector);
		_session->saveSettingsDelayed();
		closeBox();
	});
	addButton(tr::lng_close(), [=] { closeBox(); });

	setDimensionsToContent(st::boxWidth, inner);

	inner->heightValue(
	) | rpl::start_with_next([=](int height) {
		using std::min;
		setDimensions(st::boxWidth, min(height, st::boxMaxListHeight));
	}, inner->lifetime());
}

} // namespace Ui

#endif // !TDESKTOP_DISABLE_SPELLCHECK