#include <spine/spine-sfml.h>
#include <spine/spine.h>
#include <spine/extension.h>
#include <spine/util.h>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderStates.hpp>

using sf::Quads;
using sf::RenderTarget;
using sf::RenderStates;
using sf::Texture;
using sf::Uint8;
using sf::Vertex;
using sf::VertexArray;

namespace spine {

void _SfmlAtlasPage_dispose (AtlasPage* page) {
	SfmlAtlasPage* self = (SfmlAtlasPage*)page;
	_AtlasPage_deinit(&self->super);
	delete self->texture;
	FREE(page)
}

AtlasPage* AtlasPage_create (const char* name) {
	SfmlAtlasPage* self = CALLOC(SfmlAtlasPage, 1)
	_AtlasPage_init(&self->super, name);
	self->super._dispose = _SfmlAtlasPage_dispose;

	self->texture = new Texture();
	self->texture->loadFromFile(name);

	return &self->super;
}

/**/

void _SfmlSkeleton_dispose (Skeleton* skeleton) {
	SfmlSkeleton* self = (SfmlSkeleton*)skeleton;
	_Skeleton_deinit(&self->super);

	delete self->vertexArray;
	delete self->drawable;

	FREE(self)
}

Skeleton* Skeleton_create (SkeletonData* data) {
	SfmlSkeleton* self = CALLOC(SfmlSkeleton, 1)
	_Skeleton_init(&self->super, data);
	self->super._dispose = _SfmlSkeleton_dispose;

	self->drawable = new SkeletonDrawable(&self->super);
	self->vertexArray = new VertexArray(Quads, data->boneCount * 4);

	return &self->super;
}

SkeletonDrawable& Skeleton_getDrawable (const Skeleton* self) {
	return *((SfmlSkeleton*)self)->drawable;
}

SkeletonDrawable::SkeletonDrawable (Skeleton* self) {
	skeleton = (SfmlSkeleton*)self;
}

void SkeletonDrawable::draw (RenderTarget& target, RenderStates states) const {
	skeleton->vertexArray->clear();
	for (int i = 0; i < skeleton->super.slotCount; ++i)
		if (skeleton->super.slots[i]->attachment) ; //skeleton->slots[i]->attachment->draw(slots[i]);
	// BOZO - Draw the slots!
	states.texture = skeleton->texture;
	target.draw(*skeleton->vertexArray, states);
}

/**/

void _SfmlRegionAttachment_dispose (Attachment* self) {
	_RegionAttachment_deinit((RegionAttachment*)self);
	FREE(self)
}

RegionAttachment* RegionAttachment_create (const char* name, AtlasRegion* region) {
	SfmlRegionAttachment* self = CALLOC(SfmlRegionAttachment, 1)
	_RegionAttachment_init(&self->super, name);
	self->super.super._dispose = _SfmlRegionAttachment_dispose;

	self->texture = ((SfmlAtlasPage*)region->page)->texture;
	int u = region->x;
	int u2 = u + region->width;
	int v = region->y;
	int v2 = v + region->height;
	if (region->rotate) {
		self->vertices[1].texCoords.x = u;
		self->vertices[1].texCoords.y = v2;
		self->vertices[2].texCoords.x = u;
		self->vertices[2].texCoords.y = v;
		self->vertices[3].texCoords.x = u2;
		self->vertices[3].texCoords.y = v;
		self->vertices[0].texCoords.x = u2;
		self->vertices[0].texCoords.y = v2;
	} else {
		self->vertices[0].texCoords.x = u;
		self->vertices[0].texCoords.y = v2;
		self->vertices[1].texCoords.x = u;
		self->vertices[1].texCoords.y = v;
		self->vertices[2].texCoords.x = u2;
		self->vertices[2].texCoords.y = v;
		self->vertices[3].texCoords.x = u2;
		self->vertices[3].texCoords.y = v2;
	}

	return &self->super;
}

void _RegionAttachment_draw (SfmlRegionAttachment* self, Slot* slot) {
	SfmlSkeleton* skeleton = (SfmlSkeleton*)slot->skeleton;
	Uint8 r = skeleton->super.r * slot->r * 255;
	Uint8 g = skeleton->super.g * slot->g * 255;
	Uint8 b = skeleton->super.b * slot->b * 255;
	Uint8 a = skeleton->super.a * slot->a * 255;
	sf::Vertex* vertices = self->vertices;
	vertices[0].color.r = r;
	vertices[0].color.g = g;
	vertices[0].color.b = b;
	vertices[0].color.a = a;
	vertices[1].color.r = r;
	vertices[1].color.g = g;
	vertices[1].color.b = b;
	vertices[1].color.a = a;
	vertices[2].color.r = r;
	vertices[2].color.g = g;
	vertices[2].color.b = b;
	vertices[2].color.a = a;
	vertices[3].color.r = r;
	vertices[3].color.g = g;
	vertices[3].color.b = b;
	vertices[3].color.a = a;

	float* offset = self->super.offset;
	Bone* bone = slot->bone;
	vertices[0].position.x = offset[0] * bone->m00 + offset[1] * bone->m01 + bone->worldX;
	vertices[0].position.y = offset[0] * bone->m10 + offset[1] * bone->m11 + bone->worldY;
	vertices[1].position.x = offset[2] * bone->m00 + offset[3] * bone->m01 + bone->worldX;
	vertices[1].position.y = offset[2] * bone->m10 + offset[3] * bone->m11 + bone->worldY;
	vertices[2].position.x = offset[4] * bone->m00 + offset[5] * bone->m01 + bone->worldX;
	vertices[2].position.y = offset[4] * bone->m10 + offset[5] * bone->m11 + bone->worldY;
	vertices[3].position.x = offset[6] * bone->m00 + offset[7] * bone->m01 + bone->worldX;
	vertices[3].position.y = offset[6] * bone->m10 + offset[7] * bone->m11 + bone->worldY;

	// SMFL doesn't handle batching for us, so we'll just force a single texture per skeleton.
	skeleton->texture = self->texture;
	skeleton->vertexArray->append(vertices[0]);
	skeleton->vertexArray->append(vertices[1]);
	skeleton->vertexArray->append(vertices[2]);
	skeleton->vertexArray->append(vertices[3]);
}

}
