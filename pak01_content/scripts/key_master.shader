// Master Key - silver/gold spiral blend
// Body shader: gold base with silver scrolling overlay
models/powerups/keys/key_master
{
	{
		map models/powerups/keys/key_gold.tga
		rgbGen identity
	}
	{
		map models/powerups/keys/key_silver.tga
		blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave sin 0.5 0.5 0 0.4
		tcMod rotate 30
		tcMod scroll 0.1 0.2
	}
	{
		map models/powerups/keys/envmap-y.tga
		blendfunc GL_ONE GL_ONE
		rgbGen wave sin 0.1 0.08 0 0.3
		tcGen environment
	}
	{
		map models/powerups/keys/envmap-b.tga
		blendfunc GL_ONE GL_ONE
		rgbGen wave sin 0.1 0.08 0.5 0.3
		tcGen environment
	}
}

// Master Key snake decoration: alternating silver/gold
models/powerups/keys/key_master_snake
{
	{
		map models/powerups/keys/key_gold_snake.tga
		rgbGen identity
	}
	{
		map models/powerups/keys/key_silver_snake.tga
		blendfunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen wave sin 0.5 0.5 0.25 0.6
		tcMod scroll 0 0.5
	}
}
