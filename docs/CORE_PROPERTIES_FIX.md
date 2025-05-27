# Core Node Properties Support Update

## Issue Fixed
The content migration feature was not recognizing core Drupal node properties like `created`, `status`, `uid`, etc. because it was only looking for Field API fields. These core properties are stored directly in the `node` table and are essential for proper content migration.

## What Was Added

### 1. Complete Core Node Properties Support
Updated `drupalbuddy_get_content_type_fields()` to include all core node table properties:

- **nid** - Node ID
- **vid** - Version ID  
- **type** - Content Type
- **language** - Language
- **title** - Title
- **uid** - Author ID
- **status** - Published Status (0 = unpublished, 1 = published)
- **created** - Created Date (Unix timestamp)
- **changed** - Modified Date (Unix timestamp)
- **comment** - Comment Setting
- **promote** - Promoted to Front Page
- **sticky** - Sticky at Top
- **tnid** - Translation Node ID
- **translate** - Translation Flag
- **uuid** - UUID
- **rh_action** - Rabbit Hole Action
- **rh_redirect** - Rabbit Hole Redirect
- **rh_redirect_response** - Rabbit Hole Response

### 2. Enhanced Migration Logic
Updated the batch processing to properly handle core properties:

- **Read-only Properties**: `nid`, `vid`, `type` cannot be overridden during migration
- **Smart Mapping**: Distinguishes between core properties and Field API fields
- **Default Fallbacks**: Sets sensible defaults for core properties before applying CSV mappings

### 3. Updated CSV Templates
Created corrected CSV mapping files that include core node properties:

- `content_migration_mapping_template.csv` - Updated with core properties
- `article_to_whitepaper_migration_corrected.csv` - Your specific mapping corrected

## Your Corrected CSV Mapping

```csv
source_field,target_field,action
nid,nid,ignore
title,title,map
body,field_whitepaper_summary,map
field_image,field_whitepaper_cover,map
field_companies,field_companies,map
created,created,map
changed,changed,map
status,status,map
uid,uid,map
language,language,map
comment,comment,map
promote,promote,ignore
sticky,sticky,ignore
translate,translate,ignore
tnid,tnid,ignore
uuid,uuid,ignore
```

## Key Changes Explained

### Core Property Mapping
- **created/status**: Now properly recognized and can be mapped
- **uid**: Author information preserved during migration
- **language**: Language settings maintained
- **comment**: Comment settings transferred

### Smart Ignoring
- **nid**: Ignored because new nodes get new IDs
- **promote/sticky**: Set to ignore if you don't want whitepapers promoted
- **translate/tnid/uuid**: Generally ignored unless you need translation support

### Migration Safety
- Core properties are set with defaults first, then overridden by CSV mapping
- Read-only properties (nid, vid, type) are protected from accidental mapping
- Field data structure is preserved for complex fields

## Testing Your Migration

Now when you preview your migration, you should see:
- ✅ `created` field recognized as "Ready to map"
- ✅ `status` field recognized as "Ready to map"
- ✅ All other core properties properly detected

The migration will now preserve:
- Creation dates from your original articles
- Publishing status (published/unpublished)
- Author assignments
- Language settings

## Files Updated
- `drupalbuddy.admin.inc` - Enhanced field detection and migration logic
- `content_migration_mapping_template.csv` - Updated template
- `article_to_whitepaper_migration_corrected.csv` - Your corrected mapping

You can now use the corrected CSV file for your Article to Whitepaper migration and all core node properties will be properly recognized and migrated!
