#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// Etapas: {ticks para todo, ticks para wave}. {0,0} = OFF (input crudo).
static constexpr int STAGES[5][2] = {{2, 2}, {4, 4}, {8, 8}, {16, 8}, {0, 0}};
static constexpr int STAGE_COUNT = 5;
static int g_stage = 0;

static std::string stageText() {
	if (g_stage >= STAGE_COUNT - 1) return "Buf OFF";
	return fmt::format("Buf {}/{}", STAGES[g_stage][0], STAGES[g_stage][1]);
}

class $modify(BufferLayer, GJBaseGameLayer) {
	struct Fields {
		bool injecting = false;
		int left[2] = {0, 0};
		bool held[2] = {false, false};
		bool releaseDeferred[2] = {false, false};
		bool prevGround[2] = {false, false};
		double prevVel[2] = {0, 0};
	};

	PlayerObject* playerAt(int i) { return i == 0 ? m_player1 : m_player2; }

	bool unbuffered(PlayerObject* p) {
		return !p || p->m_isDart || p->m_isShip || p->m_isBird || p->m_isDead;
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();
		int i = isPlayer1 ? 0 : 1;
		auto pl = playerAt(i);

		if (f->injecting || button != 1 || g_stage >= STAGE_COUNT - 1 || unbuffered(pl)) {
			return GJBaseGameLayer::handleButton(down, button, isPlayer1);
		}

		if (down) {
			f->left[i] = STAGES[g_stage][0];
			f->held[i] = true;
			f->releaseDeferred[i] = false;
			f->prevGround[i] = pl->m_isOnGround;
			f->prevVel[i] = pl->m_yVelocity;
			GJBaseGameLayer::handleButton(true, button, isPlayer1);
		} else {
			f->held[i] = false;
			if (f->left[i] > 0) {
				f->releaseDeferred[i] = true;
				return;
			}
			GJBaseGameLayer::handleButton(false, button, isPlayer1);
		}
	}

	void processCommands(float dt) {
		auto f = m_fields.self();

		for (int i = 0; i < 2; i++) {
			if (f->left[i] <= 0) continue;
			auto pl = playerAt(i);
			bool p1 = (i == 0);

			if (unbuffered(pl)) {
				f->left[i] = 0;
				continue;
			}

			double dir = pl->m_isUpsideDown ? -1.0 : 1.0;
			bool leftGround = f->prevGround[i] && !pl->m_isOnGround;
			bool boosted = dir * (pl->m_yVelocity - f->prevVel[i]) > 4.0;

			if (leftGround || boosted) {
				f->left[i] = 0;
			} else {
				f->left[i]--;
				if (f->left[i] > 0) {
					f->injecting = true;
					GJBaseGameLayer::handleButton(false, 1, p1);
					GJBaseGameLayer::handleButton(true, 1, p1);
					f->injecting = false;
				}
			}

			f->prevGround[i] = pl->m_isOnGround;
			f->prevVel[i] = pl->m_yVelocity;

			if (f->left[i] <= 0 && f->releaseDeferred[i]) {
				f->releaseDeferred[i] = false;
				f->injecting = true;
				GJBaseGameLayer::handleButton(false, 1, p1);
				f->injecting = false;
			}
		}

		GJBaseGameLayer::processCommands(dt);
	}
};

class $modify(BufferPlayLayer, PlayLayer) {
	struct Fields {
		CCLabelBMFont* label = nullptr;
	};

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		g_stage = Mod::get()->get
