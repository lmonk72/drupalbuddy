# DrupalBuddy Content Migration Feature

## Overview
The Content Migration feature allows you to migrate content from one content type to another with custom field mappings using a CSV configuration file.

## Features
- **CSV-based Field Mapping**: Define which fields map between source and target content types
- **Preview Functionality**: See exactly what will be migrated before executing
- **Batch Processing**: Handles large numbers of content nodes efficiently
- **Field Validation**: Verifies that source and target fields exist
- **Comprehensive Logging**: Tracks all migration activities in Drupal's watchdog

## How to Use

### 1. Access the Migration Tool
Navigate to: **Administration → Reports → DrupalBuddy → Content Migration**

### 2. Prepare Your CSV Mapping File
Create a CSV file with the following columns:
- `source_field`: The field name in the source content type
- `target_field`: The field name in the target content type  
- `action`: Either "map" (to migrate the field) or "ignore" (to skip the field)

### 3. Example CSV Format
```csv
source_field,target_field,action
title,title,map
body,body,map
field_article_image,field_whitepaper_image,map
field_article_category,field_whitepaper_category,map
field_article_tags,field_whitepaper_keywords,map
field_author,field_whitepaper_author,map
field_published_date,field_whitepaper_date,map
field_article_summary,field_whitepaper_summary,map
field_article_downloads,field_download_count,ignore
field_internal_notes,field_internal_notes,ignore
```

### 4. Migration Process
1. **Select Source Content Type**: Choose the content type you want to migrate FROM
2. **Select Target Content Type**: Choose the content type you want to migrate TO
3. **Upload CSV File**: Upload your field mapping CSV file
4. **Preview**: Click "Preview Mapping" to see:
   - How many nodes will be migrated
   - Field mapping validation
   - Any potential issues
5. **Execute**: Click "Execute Migration" to start the batch process

### 5. What Happens During Migration
- **Creates New Nodes**: New nodes are created with the target content type
- **Maps Fields**: Fields are mapped according to your CSV configuration
- **Preserves Metadata**: Author, creation date, and other metadata are preserved
- **Unpublishes Originals**: Original nodes are set to unpublished (not deleted)
- **Logs Activity**: All activities are logged to Drupal's watchdog

## Field Mapping Guidelines

### Standard Node Fields
These fields are always available:
- `title`: Node title
- `body`: Main content body
- `nid`: Node ID (read-only)
- `path`: URL path/alias

### Custom Fields
Custom fields follow the pattern `field_[field_name]`:
- Example: `field_article_image`, `field_category`, `field_tags`

### Field Types Support
The migration supports all Drupal field types:
- Text fields
- Textarea/Long text
- Image fields
- File fields
- Reference fields (node, user, taxonomy)
- Date fields
- Number fields
- Boolean fields

## Example: Article to Whitepaper Migration

### Scenario
Migrating "Article" content type to "Whitepaper" content type.

### Sample CSV Mapping
```csv
source_field,target_field,action
title,title,map
body,body,map
field_article_image,field_whitepaper_image,map
field_category,field_whitepaper_category,map
field_tags,field_keywords,map
field_author,field_whitepaper_author,map
field_published_date,field_whitepaper_date,map
field_summary,field_executive_summary,map
field_download_count,field_ignore_me,ignore
```

### Process
1. All "Article" nodes will be duplicated as "Whitepaper" nodes
2. Title and body content will be preserved
3. Images will move from `field_article_image` to `field_whitepaper_image`
4. Categories and tags will be remapped to whitepaper equivalents
5. Download count will be ignored (not migrated)
6. Original articles will remain but be unpublished

## Troubleshooting

### Common Issues
1. **"Source field not found"**: The field doesn't exist in the source content type
2. **"Target field not found"**: The field doesn't exist in the target content type
3. **"Invalid action"**: Action must be either "map" or "ignore"

### Best Practices
1. **Test First**: Always use the preview function before executing
2. **Backup Database**: Create a database backup before large migrations
3. **Small Batches**: Test with a few nodes first
4. **Field Verification**: Verify field names using admin/structure/types
5. **Check Logs**: Monitor watchdog logs during migration

## Technical Notes

### Performance
- Processes 5 nodes per batch to prevent timeouts
- Uses Drupal's batch API for reliable processing
- Memory efficient for large datasets

### Data Integrity
- Original nodes are preserved (unpublished, not deleted)
- All field data is copied, not moved
- Maintains referential integrity for entity references

### Permissions
- Requires "administer drupalbuddy" permission
- Migration runs with admin privileges

## Template File
A sample CSV template is included: `content_migration_mapping_template.csv`
