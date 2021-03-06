/*
 *  Copyright (C) 2017 the authors (see AUTHORS)
 *
 *  This file is part of Kitten Keeper.
 *
 *  Kitten Keeper is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Kitten Keeper is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kitten Keeper.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <functional>

#include <lair/core/json.h>

#include "ui/label.h"

#include "game.h"
#include "level.h"
#include "game_view.h"
#include "toy_button.h"
#include "splash_state.h"

#include "main_state.h"


#define ONE_SEC (1000000000)


const float TICK_LENGTH_IN_SEC = 1.f / float(TICKS_PER_SEC);
const float FADE_DURATION = .5;
const float KITTEN_TIME = 20;

void dumpEntityTree(Logger& log, EntityRef e, unsigned indent = 0) {
	log.info(std::string(indent * 2u, ' '), e.name(), ": ", e.isEnabled(), ", ", e.position3().transpose());
	EntityRef c = e.firstChild();
	while(c.isValid()) {
		dumpEntityTree(log, c, indent + 1);
		c = c.nextSibling();
	}
}


MainState::MainState(Game* game)
	: GameState(game),

      _mainPass(renderer()),
      _guiPass(renderer()),

      _entities(log(), _game->serializer()),
      _spriteRenderer(loader(), renderer()),

      _sprites(assets(), loader(), &_mainPass, &_spriteRenderer),
      _collisions(),
      _triggers(),
      _kittens(this),
      _toys(this),
      _texts(loader(), &_mainPass, &_spriteRenderer),
      _tileLayers(loader(), &_mainPass, &_spriteRenderer),

      _inputs(sys(), &log()),

      _gui(sys(), assets(), loader(), &_spriteRenderer),

      _camera(),

      _initialized(false),
      _running(false),
      _loop(sys()),
      _fpsTime(0),
      _fpsCount(0),

      _quitInput(nullptr),
      _leftInput(nullptr),
      _rightInput(nullptr),
      _downInput(nullptr),
      _upInput(nullptr),
      _okInput(nullptr),
      _foodInput(nullptr),
      _toyInput(nullptr),
      _litterInput(nullptr),
      _medecineInput(nullptr),
      _basketInput(nullptr),

      _state(STATE_PLAY),

      _gameView(nullptr),
      _menu(nullptr),
      _foodButton(nullptr),
      _toyButton(nullptr),
      _litterButton(nullptr),
      _pillButton(nullptr),
      _basketButton(nullptr),
      _happinessLabel(nullptr),
      _moneyLabel(nullptr),
      _catLabel(nullptr),
      _toyButtonPos(8, 8),
      _dialog(nullptr),
      _dialogText(nullptr),
      _dialogButton(nullptr)
{
	_entities.registerComponentManager(&_sprites);
	_entities.registerComponentManager(&_collisions);
	_entities.registerComponentManager(&_triggers);
	_entities.registerComponentManager(&_kittens);
	_entities.registerComponentManager(&_toys);
	_entities.registerComponentManager(&_texts);
	_entities.registerComponentManager(&_tileLayers);
}


MainState::~MainState() {
}


void MainState::initialize() {
	srand(time(nullptr));

	_loop.reset();
	_loop.setTickDuration(    ONE_SEC /  TICKS_PER_SEC);
//	_loop.setFrameDuration(   ONE_SEC /  FRAMES_PER_SEC);
	_loop.setFrameDuration(   0);
	_loop.setMaxFrameDuration(_loop.frameDuration() * 3);
	_loop.setFrameMargin(     _loop.frameDuration() / 2);

	window()->onResize.connect(std::bind(&MainState::resizeEvent, this))
	        .track(_slotTracker);

	_quitInput  = _inputs.addInput("quit");
	_leftInput  = _inputs.addInput("left");
	_rightInput = _inputs.addInput("right");
	_downInput  = _inputs.addInput("down");
	_upInput    = _inputs.addInput("up");
	_okInput    = _inputs.addInput("ok");
	_foodInput     = _inputs.addInput("food");
	_toyInput      = _inputs.addInput("toy");
	_litterInput   = _inputs.addInput("litter");
	_medecineInput = _inputs.addInput("medecine");
	_basketInput   = _inputs.addInput("basket");

	_inputs.mapScanCode(_quitInput,  SDL_SCANCODE_ESCAPE);
	_inputs.mapScanCode(_leftInput,  SDL_SCANCODE_LEFT);
	_inputs.mapScanCode(_rightInput, SDL_SCANCODE_RIGHT);
	_inputs.mapScanCode(_downInput,  SDL_SCANCODE_DOWN);
	_inputs.mapScanCode(_upInput,    SDL_SCANCODE_UP);
	_inputs.mapScanCode(_okInput,    SDL_SCANCODE_SPACE);
	_inputs.mapScanCode(_okInput,    SDL_SCANCODE_RETURN);
	_inputs.mapScanCode(_okInput,    SDL_SCANCODE_RETURN2);

	_inputs.mapScanCode(_foodInput,     SDL_SCANCODE_Q);
	_inputs.mapScanCode(_toyInput,      SDL_SCANCODE_W);
	_inputs.mapScanCode(_litterInput,   SDL_SCANCODE_E);
	_inputs.mapScanCode(_medecineInput, SDL_SCANCODE_R);
	_inputs.mapScanCode(_basketInput,   SDL_SCANCODE_T);

	// TODO: load stuff.
	loadEntities("entities.ldl", _entities.root());

	// TODO[Doc]: Here is how to fetch an entity defined in entities.ldl
	_models       = _entities.findByName("__models__");
	_kittenModel  = _entities.findByName("kitten_model");
	_foodModel    = _entities.findByName("food_model");
	_toyModel     = _entities.findByName("toy_model");
	_litterModel  = _entities.findByName("litter_model");
	_pillModel    = _entities.findByName("pill_model");
	_basketModel  = _entities.findByName("basket_model");

	_scene       = _entities.findByName("scene");

	loadSound("kittendeath.wav");
	loadSound("kittenmeow1.wav");
	loadSound("kittenmeow2.wav");
	loadSound("kittenmeow3.wav");

	loadMusic("ending.mp3");

//	loader()->load<ImageLoader>("battery1.png");

	AssetSP font = loader()->loadAsset<BitmapFontLoader>("droid_sans_24.json");

	// We need the font to properly resize the widgets.
	loader()->waitAll();

	_gui.setLogicScreenSize(Vector2(1920, 1080));

	_gameView = _gui.createWidget<GameView>();
	_gameView->setMainState(this);
	_gameView->setName("game_view");

	_menu = _gui.createWidget<Widget>();
	_menu->setName("menu");
	_menu->resize(Vector2(1920, 96));
	_menu->setFrameTexture("white.png");
	_menu->setFrameColor(srgba(.8, .6, .3, 1));

	_foodButton   = createToyButton(_foodModel, "Food (Q)", "gamelle.png",
	                                "Because kittens need to eat. Can I haz Cheezburger ?");
	_toyButton    = createToyButton(_toyModel, "Toy (W)", "jouet.png",
	                                "Yaaay ! Toy ! Everything is a Toy. I toy. You toy.");
	_litterButton = createToyButton(_litterModel, "Litter (E)", "litiere.png",
	                                "Because kittens need to ***.");
	_pillButton   = createToyButton(_pillModel, "Medecine (R)", "medoc.png",
	                                "Heal sick kittens. Also, DRUUUUUUUUUUG !");
	_basketButton = createToyButton(_basketModel, "Basket (T)", "paniere.png",
	                                "If I fit, I sleep.");

	_happinessLabel = _menu->createChild<Label>();
	_happinessLabel->setName("happiness");
	_happinessLabel->setFont(font);
	_happinessLabel->textInfo().setColor(srgba(0, 0, 0, 1));
	_happinessLabel->setText("Happiness: 100%");
	_happinessLabel->resizeToText();
	_toyButtonPos(0) += 24;
	_happinessLabel->place(_toyButtonPos + Vector2(0, 80 - _happinessLabel->size()(1)) / 2);
	_toyButtonPos(0) += _happinessLabel->size()(0) + 32;

	_moneyLabel = _menu->createChild<Label>();
	_moneyLabel->setName("money");
	_moneyLabel->setFont(font);
	_moneyLabel->textInfo().setColor(srgba(0, 0, 0, 1));
	_moneyLabel->setText("Money: 999999$");
	_moneyLabel->resizeToText();
	_moneyLabel->place(_toyButtonPos + Vector2(0, 80 - _moneyLabel->size()(1)) / 2);
	_toyButtonPos(0) += _moneyLabel->size()(0) + 32;

	_catLabel = _menu->createChild<Label>();
	_catLabel->setName("cat");
	_catLabel->setFont(font);
	_catLabel->textInfo().setColor(srgba(0, 0, 0, 1));
	_catLabel->setText("Cats: 999, Deaths: 999");
	_catLabel->resizeToText();
	_catLabel->place(_toyButtonPos + Vector2(0, 80 - _catLabel->size()(1)) / 2);
	_toyButtonPos(0) += _catLabel->size()(0) + 32;

	_dialog = _gui.createWidget<Widget>();
	_dialog->setEnabled(false);
	_dialog->setFrameTexture("frame.png");
	_dialog->setFrameColor(srgba(0, 0, 0, .8));

	_dialogText = _dialog->createChild<Label>();
	_dialogText->setFont(font);
	_dialogText->textInfo().setColor(srgba(1, 1, 1, 1));

	_dialogButton = _dialog->createChild<Label>();
	_dialogButton->setFrameTexture("frame.png");
	_dialogButton->setFrameColor(srgba(.5, .6, .7, 1));
	_dialogButton->setFont(font);
	_dialogButton->textInfo().setColor(srgba(1, 1, 1, 1));
	_dialogButton->setMargin(32, 16);
	_dialogButton->onMouseUp = std::bind(&MainState::closeDialog, this);
	_dialogButton->onMouseEnter = [this](Widget*, HoverEvent& e) {
		_dialogButton->setFrameColor(srgba(.6, .7, .8, 1));
		e.accept();
	};
	_dialogButton->onMouseLeave = [this](Widget*, HoverEvent& e) {
		_dialogButton->setFrameColor(srgba(.5, .6, .7, 1));
		e.accept();
	};

	loader()->waitAll();

	// Set to true to debug OpenGL calls
//	renderer()->context()->setLogCalls(true);

	_initialized = true;
}


void MainState::shutdown() {
	_slotTracker.disconnectAll();

	_initialized = false;
}


void MainState::run() {
	lairAssert(_initialized);

	log().log("Starting main state...");
	_running = true;
	_loop.start();
	_fpsTime  = int64(sys()->getTimeNs());
	_fpsCount = 0;

	startGame();

	do {
		switch(_loop.nextEvent()) {
		case InterpLoop::Tick:
			updateTick();
			break;
		case InterpLoop::Frame:
			updateFrame();
			break;
		}
	} while (_running);
	_loop.stop();
}


void MainState::exec(const std::string& cmds, EntityRef self) {
	CommandList commands;
	unsigned first = 0;
	for(unsigned ci = 0; ci <= cmds.size(); ++ci) {
		if(ci == cmds.size() || cmds[ci] == '\n' || cmds[ci] == ';') {
			commands.emplace_back(CommandExpr{ String(cmds.begin() + first, cmds.begin() + ci), self });
			first = ci + 1;
		}
	}
	exec(commands);
}


void MainState::exec(const CommandList& commands) {
	bool execNow = _commandList.empty();
	_commandList.insert(_commandList.begin(), commands.begin(), commands.end());
	if(execNow)
		execNext();
}


void MainState::execNext() {
	while(!_commandList.empty()) {
		CommandExpr cmd = _commandList.front();
		_commandList.pop_front();
		if(execSingle(cmd.command, cmd.self) == 0)
			return;
	}
}


int MainState::execSingle(const std::string& cmd, EntityRef self) {
#define MAX_CMD_ARGS 32

	std::string tokens = cmd;
	unsigned    size   = tokens.size();
	int ret = 0;
	for(unsigned ci = 0; ci < size; ) {
		int   argc = 0;
		const char* argv[MAX_CMD_ARGS];
		while(ci < size) {
			bool endLine = false;
			while(ci < size && std::isspace(tokens[ci])) {
				endLine = (tokens[ci] == '\n') || (tokens[ci] == ';');
				tokens[ci] = '\0';
				++ci;
			}
			if(endLine)
				break;

			argv[argc] = tokens.data() + ci;
			++argc;

			while(ci < size && !std::isspace(tokens[ci])) {
				++ci;
			}
		}

		if(argc) {
			ret = exec(argc, argv, self);
		}
	}

	return ret;
}


int MainState::exec(int argc, const char** argv, EntityRef self) {
	lairAssert(argc > 0);

	std::ostringstream out;
	out << argv[0];
	for(int i = 1; i < argc; ++i)
		out << " " << argv[i];
	dbgLogger.info(out.str());

	auto cmd = _commands.find(argv[0]);
	if(cmd == _commands.end()) {
		dbgLogger.warning("Unknown command \"", argv[0], "\"");
		return -1;
	}
	return cmd->second(this, self, argc, argv);
}


void MainState::quit() {
	_running = false;
}


Game* MainState::game() {
	return static_cast<Game*>(_game);
}


void MainState::setLevel(const Path& level) {
	_levelPath = level;

	registerLevel(level);
	loader()->waitAll();
}


LevelSP MainState::registerLevel(const Path& path) {
	LevelSP level(new Level(this, path));
	_levelMap.emplace(path, level);
	level->preload();

	return level;
}


void MainState::loadLevel(const Path& level) {
	if(_level)
		_level->stop();

	while(_scene.firstChild().isValid())
		_scene.firstChild().destroy();

	_level = _levelMap.at(level);
	_level->initialize();

	EntityRef layer = _entities.findByName("layer_base");
	auto tileLayer = _tileLayers.get(layer);
	tileLayer->setBlendingMode(BLEND_ALPHA);
//	tileLayer->setTextureFlags(Texture::BILINEAR_NO_MIPMAP | Texture::CLAMP);

	layer.setEnabled(true);

	_toyLayer = _entities.createEntity(_scene, "toy_layer");
	_toyLayer.placeAt(Vector3(0, 0, 0.1));

	_kittenLayer = _entities.createEntity(_scene, "kitten_layer");
	_kittenLayer.placeAt(Vector3(0, 0, 0.2));

	_level->start();
}


void MainState::loadSound(const Path& sound) {
	loader()->load<SoundLoader>(sound);
}


void MainState::playSound(const Path& sound) {
	AssetSP asset = assets()->getAsset(sound);
	auto aspect = asset->aspect<SoundAspect>();
	aspect->_get().setVolume(game()->config().soundVolume);
	audio()->playSound(asset);
}


void MainState::loadMusic(const Path& sound) {
	loader()->load<MusicLoader>(sound);
}


void MainState::playMusic(const Path& sound) {
	AssetSP asset = assets()->getAsset(sound);
	audio()->setMusicVolume(0.5);
	audio()->playMusic(assets()->getAsset(sound));
}


ToyButton* MainState::createToyButton(EntityRef model, const String& name,
                                      const String& picture, const String& description) {
	ToyComponent* toy = _toys.get(model);
	int cost = toy? toy->cost: 10;

	ToyButton* button = _gui.createWidget<ToyButton>();
	button->place(_toyButtonPos);
	button->setMainState(this);
	button->setPicture(picture);
	button->setModel(model);
	button->setToyName(name);
	button->setDescription(cat(name, "\nCost: ", cost, "$\n\n", description));
	button->layout();

	_toyButtonPos(0) += 88;

	return button;
}


EntityRef MainState::getEntity(const String& name, const EntityRef& ancestor) {
	EntityRef entity = _entities.findByName(name, ancestor);
	if(!entity.isValid()) {
		log().error("Entity \"", name, "\" not found.");
	}
	return entity;
}


EntityRef MainState::createTrigger(EntityRef parent, const char* name, const AlignedBox2& box) {
	EntityRef entity = _entities.createEntity(parent, name);

	CollisionComponent* cc = _collisions.addComponent(entity);
	cc->addShape(Shape2D(box));
//	cc->setHitMask(HIT_PLAYER | HIT_TRIGGER);
//	cc->setIgnoreMask(HIT_TRIGGER);

	_triggers.addComponent(entity);

	return entity;
}


void MainState::updateTriggers(bool /*disableCmds*/) {
	// FIXME: Is this useful ?
//	_triggers.compactArray();

//	for(TriggerComponent& tc: _triggers) {
//		if(tc.isEnabled() && tc.entity().isEnabledRec()) {
//			tc.prevInside = tc.inside;
//			tc.inside = false;
//		}
//	}

//	for(HitEvent hit: _collisions.hitEvents()) {
//		if(hit.entities[1] == _player) {
//			std::swap(hit.entities[0], hit.entities[1]);
//		}

//		if(hit.entities[0] == _player) {
//			TriggerComponent* tc = _triggers.get(hit.entities[1]);
//			if(tc) {
//				tc->inside = true;
//			}
//		}
//	}

//	if(!disableCmds) {
//		for(TriggerComponent& tc: _triggers) {
//			if(tc.isEnabled() && tc.entity().isEnabledRec()) {
//				if(!tc.prevInside && tc.inside && !tc.onEnter.empty())
//					exec(tc.onEnter, tc.entity());
//				if(tc.prevInside && !tc.inside && !tc.onExit.empty())
//					exec(tc.onExit, tc.entity());
//			}
//		}
//	}
}

void MainState::showDialog(const String& message, const String& buttonText, State state) {
	float width = 600;
	float margin = 16;

	_dialogButton->setText(buttonText);
	_dialogButton->resizeToText();
	_dialogButton->place(Vector2(width - _dialogButton->size()(0) - margin, margin));

	_dialogText->setText(message);
	_dialogText->resizeToText(width - 2 * margin);
	_dialogText->place(Vector2(margin, _dialogButton->position()(1) + _dialogButton->size()(1) + margin));

	_dialog->resize(Vector2(width, _dialogText->position()(1) + _dialogText->size()(1) + margin));
	_dialog->place((Vector2(1920, 1080) - _dialog->size()) / 2);
	_dialog->setEnabled(true);

	_state = state;

}

void MainState::closeDialog() {
	_dialog->setEnabled(false);

	_state = STATE_PLAY;
}


void MainState::setHappiness(float happiness) {
	_happiness = happiness;
	_happinessLabel->setText(cat("Happiness: ", std::round(_happiness * 100), "%"));

	if(_happiness < 0) {
		showDialog("Ho nooo ! Things got so messy we have to close down...\n\nDon't worry, you can try again.", "NOOOoooooo...");
		_dialogButton->onMouseUp = [this](Widget*, MouseEvent& e) {
			game()->splashState()->setNextState(nullptr);
			game()->splashState()->clearSplash();
			game()->splashState()->addSplash("EndScreen.png");

			game()->setNextState(game()->splashState());
			quit();

			playMusic("ending.mp3");

			e.accept();
		};
	}
}


void MainState::setMoney(int money) {
	_money = money;
	_moneyLabel->setText(cat("Money: ", _money, "$"));

	_foodButton->update();
	_toyButton->update();
	_litterButton->update();
	_pillButton->update();
	_basketButton->update();
}

void MainState::setSpawnDeath(int spawn, int death) {
	_spawnCount = spawn;
	_deathCount = death;

	_catLabel->setText(cat("Cats: ", _spawnCount - _deathCount, ", Deaths: ", _deathCount));
}

EntityRef MainState::spawnKitten(const Vector2& pos) {
	EntityRef kitten = _entities.cloneEntity(_kittenModel, _kittenLayer, "kitten");
	setSpawnDeath(_spawnCount + 1, _deathCount);
	setMoney(_money + 20 * _happiness);

	if(pos(0) >= 0) {
		kitten.placeAt(pos);
	}
	else {
		CollisionComponent* coll = _collisions.get(kitten);
		lairAssert(coll);

		TileLayerCSP tileLayer = _level->tileMap()->tileLayer(0);

		AlignedBox2 box;
		int tries = 0;
		do {
			kitten.placeAt(Vector2(rand() % (tileLayer->widthInTiles() * TILE_SIZE),
			                       rand() % (tileLayer->heightInTiles() * TILE_SIZE)));
			box = coll->shapes()[0].transformed(kitten.worldTransform()).boundingBox();
			++tries;
		} while(_level->hitTest(box) && tries < 10);
	}

	return kitten;
}


void MainState::startGame() {
	loadLevel(_levelPath);

	_spawnCount = 0;
	_deathCount = 0;

	setHappiness(1);
	setMoney(50);
	_kittenProgress = 0;
	_payProgress = 0;

	spawnKitten(Vector2(120, 180));

	showDialog("Welcome to Kitten Keeper, a game about how taking care of OMG IT'S A KITTEN it's so cute come here kitty kitty ! Oh there's a little friend for you oh aren't they cute playing together and oh there's another, cool, I like kittens and another and wait isn't that a bit much and woah there, boys calm down and OMG THERE'S TOO MANY OF'EM and now they're getting sick they're dying by the truckload.\n\nAnd this will all be your fault. But the only winning move is to get more kittens, so what can you do ?"
	           "\n\n--------\n\n"
	           "OK. Stay calm. You need to take care of the kittens. To do this, you must buy kitten stuff (use the buttons below) and place it on the ground. Cats need:\n"
	           "- Food\n"
	           "- Toys\n"
	           "- Litter\n"
	           "- Medecine (if they are sick)\n"
	           "- A basket to sleep\n\n"
	           "Kittens cannot be to many to use the same item, so be sure to place plenty of them. If they don't have what they need, bad things happen and happiness go down ! Do your best to keep the kittens happy. Happy cats mean more cats. The more you have, the better ! (or worst ?)");

	//audio()->playMusic(assets()->getAsset("music.ogg"));
	//audio()->playSound(assets()->getAsset("sound.ogg"), 2);
}


void MainState::updateTick() {
	loader()->finalizePending();

	_inputs.sync();

	_entities.setPrevWorldTransforms();

	if(_quitInput->justPressed()) {
		quit();
	}

#ifndef NDEBUG
	if(_upInput->isPressed())
		setMoney(_money + 5);
	if(_rightInput->justPressed())
		spawnKitten();
	if(_downInput->justPressed())
		setHappiness(_happiness - 0.1);
#endif

	if(_state == STATE_PLAY) {
		if(_foodInput->justPressed())
			_gameView->createToy(_foodModel);
		if(_toyInput->justPressed())
			_gameView->createToy(_toyModel);
		if(_litterInput->justPressed())
			_gameView->createToy(_litterModel);
		if(_medecineInput->justPressed())
			_gameView->createToy(_pillModel);
		if(_basketInput->justPressed())
			_gameView->createToy(_basketModel);

		_kittens.update();
		_toys.update();

		int nKittens = _spawnCount - _deathCount;

		_kittenProgress += (0.25 + 0.75 * _happiness) * (1 + nKittens / 10.)
		                   / KITTEN_TIME * TICK_LENGTH_IN_SEC;
		if(_kittenProgress >= 1) {
			spawnKitten();
			_kittenProgress -= 1;
		}

		_payProgress += .2 * ceil(nKittens/10) * _happiness * TICK_LENGTH_IN_SEC;
		if(_payProgress >= 1) {
			setMoney(_money + 1);
			_payProgress -= 1;
		}

		_happiness += TICK_LENGTH_IN_SEC / 300.0f;
		if(_happiness > 1)
			_happiness = 1;
		setHappiness(_happiness);

		for(KittenComponent& kitten: _kittens) {
			EntityRef entity = kitten.entity();
			Vector3 p = entity.position3();
			p(2) = (1 - (p(1) / 1080)) / 10;
			entity.moveTo(p);
		}

		_entities.updateWorldTransforms();
		_collisions.findCollisions();

		// FIXME: Might be useless...
		updateTriggers();
	}
	else if(_state == STATE_PAUSE) {
//		if(_okInput->justPressed()) {
//			MouseEvent event(MouseEvent::MOUSE_UP, Vector2(), MOUSE_LEFT);
//			_dialogButton->onMouseUp(_dialogButton, event);
//		}
	}

	_entities.updateWorldTransforms();
}


void MainState::updateFrame() {
	// Update camera

	Vector3 pos(0, -42, 0);
	Box3 viewBox(pos, pos + Vector3(1920, 1080, 1));
	_camera.setViewBox(viewBox);

	// Rendering
	Context* glc = renderer()->context();

	_texts.createTextures();
	_tileLayers.createTextures();
	_gui.preRender();
	renderer()->uploadPendingTextures();
	_spriteRenderer.finalizeShaders();

	glc->clear(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);

	bool buffersFilled = false;
	while(!buffersFilled) {
		_mainPass.clear();
		_guiPass.clear();

		_spriteRenderer.beginRender();

		_sprites.render(_entities.root(), _loop.frameInterp(), _camera);
		_texts.render(_entities.root(), _loop.frameInterp(), _camera);
		_tileLayers.render(_entities.root(), _loop.frameInterp(), _camera);

		OrthographicCamera guiCamera;
		guiCamera.setViewBox(Box3(Vector3(0, 0, 0), Vector3(1920, 1080, 1)));
		_gui.render(_guiPass, guiCamera.transform());

		buffersFilled = _spriteRenderer.endRender();
	}

	_mainPass.render();

	glc->disable(gl::DEPTH_TEST);
	_guiPass.render();
	glc->enable(gl::DEPTH_TEST);


	window()->swapBuffers();
//	glc->setLogCalls(true);

	int64 now = int64(sys()->getTimeNs());
	++_fpsCount;
	int64 etime = now - _fpsTime;
	if(etime >= ONE_SEC) {
		log().info("Fps: ", _fpsCount * float(ONE_SEC) / etime);
		_fpsTime  = now;
		_fpsCount = 0;
	}
}


void MainState::resizeEvent() {
	// TODO: resize GUI.
	Box3 viewBox(Vector3(0, 0, 0),
	             Vector3(window()->width(),
	                     window()->height(), 1));
	_camera.setViewBox(viewBox);

	_gui.setRealScreenSize(Vector2(window()->width(), window()->height()));
	_gameView->resize(Vector2(window()->width(), window()->height()));
}


bool MainState::loadEntities(const Path& path, EntityRef parent, const Path& cd) {
	Path localPath = makeAbsolute(cd, path);
	log().info("Load entity \"", localPath, "\"");

	Path realPath = game()->dataPath() / localPath;
	Path::IStream in(realPath.native().c_str());
	if(!in.good()) {
		log().error("Unable to read \"", localPath, "\".");
		return false;
	}
	ErrorList errors;
	LdlParser parser(&in, localPath.utf8String(), &errors, LdlParser::CTX_MAP);

	bool success = _entities.loadEntitiesFromLdl(parser, parent);

	errors.log(log());

	return success;
}
