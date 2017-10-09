//
//  Common.cpp
//  Visualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

// This module
#include "Common.h"

// Cinder
#include <cinder/Font.h>
#include <cinder/gl/TextureFont.h>


float mx(0);
float my(0);


void tmb::drawString(std::string const & i_text, ci::Vec2f const& i_position, bool isCentered, ci::ColorA const& color)
{
	using namespace ci;
	static ci::Font mainFont = ci::Font("Arial", 48.);
	static ci::gl::TextureFontRef font = ci::gl::TextureFont::create(mainFont);

	gl::pushModelView();

	gl::translate(i_position);

	float scale = 1.;
	scale *= gl::getViewport().getWidth() / 2000.f;
	gl::scale(scale, scale);

	if (isCentered)
	{
		gl::drawStringCentered(i_text, i_position, color);
	}
	else
	{
		gl::color(color);
		font->drawString(i_text, i_position);
	}

	gl::popModelView();
}

std::ostream& tmb::operator<<(std::ostream& outstream, tmb::Quad quad)
{
	return outstream << "Quad TL"<<quad.tl << ", TR"<<quad.tr << ", BR"<<quad.br << ", BL"<<quad.bl;
}
