/*
 *  Copyright (C) 2017 the authors (see AUTHORS)
 *
 *  This file is part o kitten keeper.
 *
 *  lair is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lair is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lair.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "ui/gui.h"
#include "ui/label.h"
#include "ui/picture.h"

#include "main_state.h"
#include "game_view.h"

#include "toy_button.h"

using namespace lair;

ToyButton::ToyButton(Gui* gui, Widget* parent)
    : Widget(gui, parent)
    , _picture(createChild<Picture>())
    , _tooltip(createChild<Label>())
{
	setFrameTexture("frame.png");
	setFrameColor(srgba(.6, .5, .2, 1));

	setFont("droid_sans_24.json");

	_tooltip->setEnabled(false);
	_tooltip->setFrameTexture("frame.png");
	_tooltip->setFrameColor(srgba(0, 0, 0, .8));
	_tooltip->setMargin(16);
}

void ToyButton::setMainState(MainState* mainState) {
	_mainState = mainState;
}

void ToyButton::setPicture(const Path& logicPath) {
	_picture->setTexture(logicPath);
}

void ToyButton::setFont(const Path& logicPath) {
	_tooltip->setFont(logicPath);
}

void ToyButton::setModel(EntityRef model) {
	_model = model;
}

void ToyButton::setToyName(const String& name) {
	_toyName = name;
}

void ToyButton::setDescription(const String& description) {
	_tooltip->setText(description);
}

void ToyButton::layout() {
	Vector2 size(80, 80);
	resize(size);

	_picture->resizeToPicture();
	_picture->place((size - _picture->size()) / 2);

	_tooltip->resizeToText(600);
	_tooltip->place(Vector2(0, 88));
}

void ToyButton::update() {
	ToyComponent* toy = _mainState->_toys.get(_model);
	int cost = toy? toy->cost: 10;

	if(_mainState->_money < cost) {
		_picture->setPictureColor(srgba(.5, .5, .5, 1));
	}
	else {
		_picture->setPictureColor(srgba(1, 1, 1, 1));
	}
}

void ToyButton::mouseReleaseEvent(MouseEvent& event) {
	ToyComponent* toy = _mainState->_toys.get(_model);
	int cost = toy? toy->cost: 10;

	if(event.button() == MOUSE_LEFT && _mainState->_money >= cost
	        && _mainState->_state == STATE_PLAY) {
		_mainState->_gameView->createToy(_model);
//		EntityRef toy = _mainState->_entities.cloneEntity(
//		                    _model, _mainState->_toyLayer, _toyName.c_str());
//		Vector2 scenePos = _mainState->_gameView->sceneFromScreen(event.position());
//		_mainState->_gameView->beginGrab(toy, scenePos);
		event.accept();
		return;
	}
	event.reject();
}

void ToyButton::mouseEnterEvent(HoverEvent& event) {
	_tooltip->setEnabled(true);
	event.accept();
}

void ToyButton::mouseLeaveEvent(HoverEvent& event) {
	_tooltip->setEnabled(false);
	event.accept();
}
