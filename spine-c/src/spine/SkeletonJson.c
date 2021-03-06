#include <spine/SkeletonJson.h>
#include <math.h>
#include <stdio.h>
#include <spine/util.h>
#include <math.h>
#include <spine/Json.h>
#include <spine/RegionAttachment.h>
#include <spine/AtlasAttachmentLoader.h>

typedef struct {
	SkeletonJson json;
	int ownsLoader;
} Internal;

SkeletonJson* SkeletonJson_createWithLoader (AttachmentLoader* attachmentLoader) {
	SkeletonJson* self = (SkeletonJson*)CALLOC(Internal, 1)
	self->scale = 1;
	self->attachmentLoader = attachmentLoader;
	return self;
}

SkeletonJson* SkeletonJson_create (Atlas* atlas) {
	AtlasAttachmentLoader* attachmentLoader = AtlasAttachmentLoader_create(atlas);
	Internal* self = (Internal*)SkeletonJson_createWithLoader(&attachmentLoader->super);
	self->ownsLoader = 1;
	return &self->json;
}

void SkeletonJson_dispose (SkeletonJson* self) {
	if (((Internal*)self)->ownsLoader) AttachmentLoader_dispose(self->attachmentLoader);
	FREE(self->error)
	FREE(self)
}

void _SkeletonJson_setError (SkeletonJson* self, Json* root, const char* value1, const char* value2) {
	FREE(self->error)
	char message[256];
	strcpy(message, value1);
	int length = strlen(value1);
	if (value2) strncat(message + length, value2, 256 - length);
	MALLOC_STR(self->error, message)
	if (root) Json_dispose(root);
}

static float toColor (const char* value, int index) {
	if (strlen(value) != 8) return -1;
	value += index * 2;
	char digits[3];
	digits[0] = *value;
	digits[1] = *(value + 1);
	digits[2] = '\0';
	char* error;
	int color = strtoul(digits, &error, 16);
	if (*error != 0) return -1;
	return color / (float)255;
}

SkeletonData* SkeletonJson_readSkeletonDataFile (SkeletonJson* self, const char* path) {
	const char* data = readFile(path);
	if (!data) {
		_SkeletonJson_setError(self, 0, "Unable to read skeleton file: ", path);
		return 0;
	}
	SkeletonData* skeletonData = SkeletonJson_readSkeletonData(self, data);
	FREE(data)
	return skeletonData;
}

SkeletonData* SkeletonJson_readSkeletonData (SkeletonJson* self, const char* json) {
	FREE(self->error)
	CAST(char*, self->error) = 0;

	Json* root = Json_create(json);
	if (!root) {
		_SkeletonJson_setError(self, 0, "Invalid skeleton JSON: ", Json_getError());
		return 0;
	}

	SkeletonData* skeletonData = SkeletonData_create();
	int i, ii, iii;

	Json* bones = Json_getItem(root, "bones");
	int boneCount = Json_getSize(bones);
	skeletonData->bones = MALLOC(BoneData*, boneCount)
	for (i = 0; i < boneCount; ++i) {
		Json* boneMap = Json_getItemAt(bones, i);

		const char* boneName = Json_getString(boneMap, "name", 0);

		BoneData* parent = 0;
		const char* parentName = Json_getString(boneMap, "parent", 0);
		if (parentName) {
			parent = SkeletonData_findBone(skeletonData, parentName);
			if (!parent) {
				SkeletonData_dispose(skeletonData);
				_SkeletonJson_setError(self, root, "Parent bone not found: ", parentName);
				return 0;
			}
		}

		BoneData* boneData = BoneData_create(boneName, parent);
		boneData->length = Json_getFloat(boneMap, "parent", 0) * self->scale;
		boneData->x = Json_getFloat(boneMap, "x", 0) * self->scale;
		boneData->y = Json_getFloat(boneMap, "y", 0) * self->scale;
		boneData->rotation = Json_getFloat(boneMap, "rotation", 0);
		boneData->scaleX = Json_getFloat(boneMap, "scaleX", 1);
		boneData->scaleY = Json_getFloat(boneMap, "scaleY", 1);

		skeletonData->bones[i] = boneData;
		skeletonData->boneCount++;
	}

	Json* slots = Json_getItem(root, "slots");
	if (slots) {
		int slotCount = Json_getSize(slots);
		skeletonData->slots = MALLOC(SlotData*, slotCount)
		for (i = 0; i < slotCount; ++i) {
			Json* slotMap = Json_getItemAt(slots, i);

			const char* slotName = Json_getString(slotMap, "name", 0);

			const char* boneName = Json_getString(slotMap, "bone", 0);
			BoneData* boneData = SkeletonData_findBone(skeletonData, boneName);
			if (!boneData) {
				SkeletonData_dispose(skeletonData);
				_SkeletonJson_setError(self, root, "Slot bone not found: ", boneName);
				return 0;
			}

			SlotData* slotData = SlotData_create(slotName, boneData);

			const char* color = Json_getString(slotMap, "color", 0);
			if (color) {
				slotData->r = toColor(color, 0);
				slotData->g = toColor(color, 1);
				slotData->b = toColor(color, 2);
				slotData->a = toColor(color, 3);
			}

			Json *attachmentItem = Json_getItem(slotMap, "attachment");
			if (attachmentItem) SlotData_setAttachmentName(slotData, attachmentItem->valuestring);

			skeletonData->slots[i] = slotData;
			skeletonData->slotCount++;
		}
	}

	Json* skinsMap = Json_getItem(root, "skins");
	if (skinsMap) {
		int skinCount = Json_getSize(skinsMap);
		skeletonData->skins = MALLOC(Skin*, skinCount)
		for (i = 0; i < skinCount; ++i) {
			Json* slotMap = Json_getItemAt(skinsMap, i);
			const char* skinName = slotMap->name;
			Skin *skin = Skin_create(skinName);
			skeletonData->skins[i] = skin;
			skeletonData->skinCount++;
			if (strcmp(skinName, "default") == 0) skeletonData->defaultSkin = skin;

			int slotNameCount = Json_getSize(slotMap);
			for (ii = 0; ii < slotNameCount; ++ii) {
				Json* attachmentsMap = Json_getItemAt(slotMap, ii);
				const char* slotName = attachmentsMap->name;
				int slotIndex = SkeletonData_findSlotIndex(skeletonData, slotName);

				int attachmentCount = Json_getSize(attachmentsMap);
				for (iii = 0; iii < attachmentCount; ++iii) {
					Json* attachmentMap = Json_getItemAt(attachmentsMap, iii);
					const char* skinAttachmentName = attachmentMap->name;
					const char* attachmentName = Json_getString(attachmentMap, "name", skinAttachmentName);

					const char* typeString = Json_getString(attachmentMap, "type", "region");
					AttachmentType type;
					if (strcmp(typeString, "region") == 0)
						type = ATTACHMENT_REGION;
					else if (strcmp(typeString, "regionSequence") == 0)
						type = ATTACHMENT_REGION_SEQUENCE;
					else {
						SkeletonData_dispose(skeletonData);
						_SkeletonJson_setError(self, root, "Unknown attachment type: ", typeString);
						return 0;
					}

					Attachment* attachment = AttachmentLoader_newAttachment(self->attachmentLoader, type, attachmentName);
					if (!attachment && self->attachmentLoader->error1) {
						SkeletonData_dispose(skeletonData);
						_SkeletonJson_setError(self, root, self->attachmentLoader->error1, self->attachmentLoader->error2);
						return 0;
					}

					if (attachment->type == ATTACHMENT_REGION || attachment->type == ATTACHMENT_REGION_SEQUENCE) {
						RegionAttachment* regionAttachment = (RegionAttachment*)attachment;
						regionAttachment->x = Json_getFloat(attachmentMap, "x", 0) * self->scale;
						regionAttachment->y = Json_getFloat(attachmentMap, "y", 0) * self->scale;
						regionAttachment->scaleX = Json_getFloat(attachmentMap, "scaleX", 1);
						regionAttachment->scaleY = Json_getFloat(attachmentMap, "scaleY", 1);
						regionAttachment->rotation = Json_getFloat(attachmentMap, "rotation", 0);
						regionAttachment->width = Json_getFloat(attachmentMap, "width", 32) * self->scale;
						regionAttachment->height = Json_getFloat(attachmentMap, "height", 32) * self->scale;
						RegionAttachment_updateOffset(regionAttachment);
					}

					Skin_addAttachment(skin, slotIndex, skinAttachmentName, attachment);
				}
			}
		}
	}

	Json_dispose(root);
	return skeletonData;
}

Animation* SkeletonJson_readAnimationFile (SkeletonJson* self, const char* path, const SkeletonData *skeletonData) {
	const char* data = readFile(path);
	if (!data) {
		_SkeletonJson_setError(self, 0, "Unable to read animation file: ", path);
		return 0;
	}
	Animation* animation = SkeletonJson_readAnimation(self, data, skeletonData);
	FREE(data)
	return animation;
}

static void readCurve (CurveTimeline* timeline, int frameIndex, Json* frame) {
	Json* curve = Json_getItem(frame, "curve");
	if (!curve) return;
	if (curve->type == Json_String && strcmp(curve->valuestring, "stepped") == 0)
		CurveTimeline_setStepped(timeline, frameIndex);
	else if (curve->type == Json_Array) {
		CurveTimeline_setCurve(timeline, frameIndex, Json_getItemAt(curve, 0)->valuefloat, Json_getItemAt(curve, 1)->valuefloat,
				Json_getItemAt(curve, 2)->valuefloat, Json_getItemAt(curve, 3)->valuefloat);
	}
}

Animation* SkeletonJson_readAnimation (SkeletonJson* self, const char* json, const SkeletonData *skeletonData) {
	FREE(self->error)
	CAST(char*, self->error) = 0;

	Json* root = Json_create(json);
	if (!root) {
		_SkeletonJson_setError(self, 0, "Invalid animation JSON: ", Json_getError());
		return 0;
	}

	Json* bones = Json_getItem(root, "bones");
	int boneCount = Json_getSize(bones);

	Json* slots = Json_getItem(root, "slots");
	int slotCount = slots ? Json_getSize(slots) : 0;

	int timelineCount = 0;
	int i, ii, iii;
	for (i = 0; i < boneCount; ++i)
		timelineCount += Json_getSize(Json_getItemAt(bones, i));
	for (i = 0; i < slotCount; ++i)
		timelineCount += Json_getSize(Json_getItemAt(slots, i));
	Animation* animation = Animation_create(timelineCount);
	animation->timelineCount = 0;

	for (i = 0; i < boneCount; ++i) {
		Json* boneMap = Json_getItemAt(bones, i);

		const char* boneName = boneMap->name;

		int boneIndex = SkeletonData_findBoneIndex(skeletonData, boneName);
		if (boneIndex == -1) {
			Animation_dispose(animation);
			_SkeletonJson_setError(self, root, "Bone not found: ", boneName);
			return 0;
		}

		int timelineCount = Json_getSize(boneMap);
		for (ii = 0; ii < timelineCount; ++ii) {
			Json* timelineArray = Json_getItemAt(boneMap, ii);
			int frameCount = Json_getSize(timelineArray);
			const char* timelineType = timelineArray->name;

			if (strcmp(timelineType, "rotate") == 0) {
				RotateTimeline *timeline = RotateTimeline_create(frameCount);
				timeline->boneIndex = boneIndex;
				for (iii = 0; iii < frameCount; ++iii) {
					Json* frame = Json_getItemAt(timelineArray, iii);
					RotateTimeline_setFrame(timeline, iii, Json_getFloat(frame, "time", 0), Json_getFloat(frame, "angle", 0));
					readCurve(&timeline->super, iii, frame);
				}
				animation->timelines[animation->timelineCount++] = (Timeline*)timeline;
				animation->duration = fmaxf(animation->duration, timeline->frames[frameCount * 2 - 2]);

			} else {
				int isScale = strcmp(timelineType, "scale") == 0;
				if (isScale || strcmp(timelineType, "translate") == 0) {
					TranslateTimeline *timeline = isScale ? ScaleTimeline_create(frameCount) : TranslateTimeline_create(frameCount);
					float scale = isScale ? 1 : self->scale;
					timeline->boneIndex = boneIndex;
					for (iii = 0; iii < frameCount; ++iii) {
						Json* frame = Json_getItemAt(timelineArray, iii);
						TranslateTimeline_setFrame(timeline, iii, Json_getFloat(frame, "time", 0), Json_getFloat(frame, "x", 0) * scale,
								Json_getFloat(frame, "y", 0) * scale);
						readCurve(&timeline->super, iii, frame);
					}
					animation->timelines[animation->timelineCount++] = (Timeline*)timeline;
					animation->duration = fmaxf(animation->duration, timeline->frames[frameCount * 3 - 3]);
				} else {
					Animation_dispose(animation);
					_SkeletonJson_setError(self, 0, "Invalid timeline type for a bone: ", timelineType);
					return 0;
				}
			}
		}
	}

	if (!slots) {
		for (i = 0; i < slotCount; ++i) {
			Json* slotMap = Json_getItemAt(slots, i);
			const char* slotName = slotMap->name;

			int slotIndex = SkeletonData_findSlotIndex(skeletonData, slotName);
			if (slotIndex == -1) {
				Animation_dispose(animation);
				_SkeletonJson_setError(self, root, "Slot not found: ", slotName);
				return 0;
			}

			int timelineCount = Json_getSize(slotMap);
			for (ii = 0; ii < timelineCount; ++ii) {
				Json* timelineArray = Json_getItemAt(slotMap, ii);
				int frameCount = Json_getSize(timelineArray);
				const char* timelineType = timelineArray->name;

				if (strcmp(timelineType, "color") == 0) {
					ColorTimeline *timeline = ColorTimeline_create(frameCount);
					timeline->slotIndex = slotIndex;
					for (iii = 0; iii < frameCount; ++iii) {
						Json* frame = Json_getItemAt(timelineArray, iii);
						const char* s = Json_getString(frame, "color", 0);
						ColorTimeline_setFrame(timeline, iii, Json_getFloat(frame, "time", 0), toColor(s, 0), toColor(s, 1),
								toColor(s, 2), toColor(s, 3));
						readCurve(&timeline->super, iii, frame);
					}
					animation->timelines[animation->timelineCount++] = (Timeline*)timeline;
					animation->duration = fmaxf(animation->duration, timeline->frames[frameCount * 5 - 5]);

				} else if (strcmp(timelineType, "attachment") == 0) {
					AttachmentTimeline *timeline = AttachmentTimeline_create(frameCount);
					timeline->slotIndex = slotIndex;
					for (iii = 0; iii < frameCount; ++iii) {
						Json* frame = Json_getItemAt(timelineArray, iii);
						Json* name = Json_getItem(frame, "name");
						AttachmentTimeline_setFrame(timeline, iii, Json_getFloat(frame, "time", 0),
								name->type == Json_NULL ? 0 : name->valuestring);
					}
					animation->timelines[animation->timelineCount++] = (Timeline*)timeline;
					animation->duration = fmaxf(animation->duration, timeline->frames[frameCount - 1]);

				} else {
					Animation_dispose(animation);
					_SkeletonJson_setError(self, 0, "Invalid timeline type for a slot: ", timelineType);
					return 0;
				}
			}
		}
	}

	return animation;
}
