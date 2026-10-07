/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Catalogue for text written in C++. Phase 4, task 4.3.
 *
 *  The strings the engine already had are Lisp symbols in
 *  data/lisp/english.lsp, translated by loading another table on top of it.
 *  Text that the new UI adds cannot go there: that file is original data this
 *  project does not edit. It lives here instead, one phrase per entry, defined
 *  in the header so adding a string is one edit in one place and a translator
 *  has the whole vocabulary in front of them.
 *
 *  The Portuguese column is Latin-1, the encoding the renderer draws in, so
 *  the bytes are written as escapes and this file stays pure ASCII: no editor
 *  can silently re-encode it to UTF-8 and turn every accent into two wrong
 *  glyphs.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_I18N_UITEXT_H_
#define ABUSE_I18N_UITEXT_H_

namespace abuse::i18n {

struct Phrase
{
    char const *en;
    char const *pt;
};

// The phrase in the language in force. English for everything that has no
// translation of its own yet, which is the same fallback the Lisp table uses.
char const *say(Phrase const &p);

// ---- options screen -------------------------------------------------------

inline constexpr Phrase kOptionsTitle   = { "Options", "Op\xe7\xf5" "es" };
inline constexpr Phrase kOptionsHelp    = { "arrows change, Esc leaves",
                                            "setas mudam, Esc sai" };
inline constexpr Phrase kRestartNote    = { "* takes effect next time the game starts",
                                            "* vale na pr\xf3xima vez que o jogo abrir" };
inline constexpr Phrase kSaved          = { "saved to abuserc",
                                            "salvo no abuserc" };
inline constexpr Phrase kNotSaved       = { "could not write abuserc",
                                            "n\xe3o foi poss\xedvel escrever o abuserc" };
inline constexpr Phrase kOn             = { "on", "ligado" };
inline constexpr Phrase kOff            = { "off", "desligado" };

inline constexpr Phrase kOptMode        = { "Mode", "Modo" };
inline constexpr Phrase kModeOriginal   = { "Original", "Original" };
inline constexpr Phrase kModeRemaster   = { "Remastered", "Remasterizado" };
inline constexpr Phrase kOptAspect      = { "Picture shape", "Formato da imagem" };
inline constexpr Phrase kOptHud         = { "HUD", "HUD" };
inline constexpr Phrase kHudClassic     = { "classic", "cl\xe1" "ssico" };
inline constexpr Phrase kHudModern      = { "modern", "moderno" };
inline constexpr Phrase kOptSmooth      = { "Smooth movement", "Movimento suave" };
inline constexpr Phrase kOptLighting    = { "Lighting", "Ilumina\xe7\xe3o" };
inline constexpr Phrase kOptReduceMotion = { "Reduce motion", "Reduzir movimento" };
inline constexpr Phrase kOptParticles  = { "Particles", "Part\xed" "culas" };
inline constexpr Phrase kOptDynLight  = { "Dynamic light", "Luz din\xe2mica" };
inline constexpr Phrase kOptBrightness = { "Brightness", "Brilho" };
inline constexpr Phrase kOptPreset    = { "Preset", "Predefini\xe7\xe3o" };
inline constexpr Phrase kLightClassic   = { "classic", "cl\xe1" "ssica" };
inline constexpr Phrase kLightRgb       = { "smooth", "suave" };
inline constexpr Phrase kOptLanguage    = { "Language", "Idioma" };
inline constexpr Phrase kOptFont        = { "Font", "Fonte" };
inline constexpr Phrase kOptScaleMode   = { "Scale mode", "Modo de escala" };
inline constexpr Phrase kOptFilter      = { "Filter", "Filtro" };
inline constexpr Phrase kOptVsync       = { "Vertical sync", "Sincronia vertical" };
inline constexpr Phrase kOptFpsLimit    = { "FPS limit", "Limite de FPS" };
inline constexpr Phrase kOptDeadzone    = { "Stick deadzone", "Zona morta" };
inline constexpr Phrase kOptAimRadius   = { "Aim radius", "Raio da mira" };
inline constexpr Phrase kOptAimAssist   = { "Aim assist", "Assist\xeancia de mira" };
inline constexpr Phrase kOptRumble      = { "Rumble", "Vibra\xe7\xe3o" };
inline constexpr Phrase kOptCursorSpeed = { "Cursor speed", "Velocidade do cursor" };

// ---- controls screen ------------------------------------------------------

inline constexpr Phrase kControlsTitle  = { "Controls", "Controles" };
inline constexpr Phrase kControlsHelp   = { "Enter adds, Backspace clears, Esc leaves",
                                            "Enter adiciona, Backspace limpa, Esc sai" };
inline constexpr Phrase kPressAny       = { "press a key or a pad button, Esc cancels",
                                            "aperte uma tecla ou um bot\xe3o, Esc cancela" };
inline constexpr Phrase kUnbound        = { "not bound", "sem atalho" };
inline constexpr Phrase kNoRoomForMore  = { "no room for another one",
                                            "sem espa\xe7o para mais um" };

inline constexpr Phrase kActMoveLeft    = { "Move left", "Andar para a esquerda" };
inline constexpr Phrase kActMoveRight   = { "Move right", "Andar para a direita" };
inline constexpr Phrase kActUp          = { "Up / jump", "Cima / pular" };
inline constexpr Phrase kActDown        = { "Down / crouch", "Baixo / agachar" };
inline constexpr Phrase kActFire        = { "Fire", "Atirar" };
inline constexpr Phrase kActSpecial     = { "Special", "Especial" };
inline constexpr Phrase kActWeaponPrev  = { "Previous weapon", "Arma anterior" };
inline constexpr Phrase kActWeaponNext  = { "Next weapon", "Pr\xf3xima arma" };

// ---- classic data screen --------------------------------------------------

inline constexpr Phrase kClassicTitle   = { "The Original mode needs the classic sound",
                                            "O Modo Original precisa do som original" };
inline constexpr Phrase kClassicWhy1    = { "Levels, art and code ship with the game.",
                                            "Fases, arte e c\xf3" "digo v\xeam com o jogo." };
inline constexpr Phrase kClassicWhy2    = { "The sound and the music do not, so they come apart.",
                                            "O som e a m\xfasica n\xe3o, ent\xe3o v\xeam \xe0 parte." };
inline constexpr Phrase kClassicOpen    = { "Open the download page",
                                            "Abrir a p\xe1gina de download" };
inline constexpr Phrase kClassicScript  = { "or run scripts/fetch-classic-data.sh",
                                            "ou rode scripts/fetch-classic-data.sh" };
inline constexpr Phrase kClassicGoOn    = { "Play without the original sound",
                                            "Jogar sem o som original" };
inline constexpr Phrase kClassicOpened  = { "opened in your browser",
                                            "aberto no navegador" };
inline constexpr Phrase kClassicNoOpen  = { "no browser opened; the address is above",
                                            "nenhum navegador abriu; o endere\xe7o est\xe1 acima" };

// The download, run from here. The script is the one the packages carry;
// this only starts it and shows what it says.
inline constexpr Phrase kClassicFetch   = { "Download it now",
                                            "Baixar agora" };
inline constexpr Phrase kClassicFetching = { "downloading, this takes a minute",
                                             "baixando, leva um minuto" };
inline constexpr Phrase kClassicCancel  = { "Esc cancels the download",
                                            "Esc cancela o download" };
inline constexpr Phrase kClassicGot     = { "done: the Original mode has its sound",
                                            "pronto: o Modo Original tem o som dele" };
inline constexpr Phrase kClassicFailed  = { "it did not finish; nothing was written",
                                            "n\xe3o terminou; nada foi escrito" };

// ---- start menu -----------------------------------------------------------

inline constexpr Phrase kStartResume    = { "Resume", "Voltar ao jogo" };
inline constexpr Phrase kStartPlay      = { "New game", "Novo jogo" };
inline constexpr Phrase kStartContinue  = { "Continue", "Continuar" };
inline constexpr Phrase kStartDifficulty= { "Difficulty", "Dificuldade" };
inline constexpr Phrase kStartBrightness= { "Brightness", "Brilho" };
inline constexpr Phrase kStartVolume    = { "Volume", "Volume" };
inline constexpr Phrase kStartCredits   = { "Credits", "Cr\xe9" "ditos" };
inline constexpr Phrase kStartQuit      = { "Quit", "Sair" };
inline constexpr Phrase kStartHelp      = { "arrows move, Enter chooses",
                                            "setas movem, Enter escolhe" };

inline constexpr Phrase kDiffEasy       = { "Easy", "F\xe1" "cil" };
inline constexpr Phrase kDiffMedium     = { "Medium", "M\xe9" "dio" };
inline constexpr Phrase kDiffHard       = { "Hard", "Dif\xed" "cil" };
inline constexpr Phrase kDiffExtreme    = { "Extreme", "Extremo" };

// ---- credits --------------------------------------------------------------

inline constexpr Phrase kCreditGame     = { "The game, 1995", "O jogo, 1995" };
inline constexpr Phrase kCreditPort     = { "SDL port and free data",
                                            "Port SDL e dados livres" };
inline constexpr Phrase kCreditFork     = { "CMake and SDL3 fork",
                                            "Fork com CMake e SDL3" };
inline constexpr Phrase kCreditThis     = { "Data, licences and history",
                                            "Dados, licen\xe7" "as e hist\xf3" "ria" };
inline constexpr Phrase kCreditLicence  = { "Code GPL-2.0, game data public domain",
                                            "C\xf3" "digo GPL-2.0, dados em dom\xed" "nio p\xfa" "blico" };
inline constexpr Phrase kCreditBack     = { "Esc goes back", "Esc volta" };

// ---- notices --------------------------------------------------------------

inline constexpr Phrase kPadLost        = { "Controller disconnected",
                                            "Controle desconectado" };
inline constexpr Phrase kPadLostHelp    = { "plug it back in, or press space",
                                            "conecte de novo, ou aperte espa\xe7o" };
inline constexpr Phrase kMenuHint       = { "F2 options    F3 controls",
                                            "F2 op\xe7\xf5" "es    F3 controles" };

}

#endif
