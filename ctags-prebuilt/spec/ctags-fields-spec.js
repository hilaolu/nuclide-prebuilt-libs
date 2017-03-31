'use strict';

var path = require('path');
var ctags = require('../');

var tagsFile = path.join(__dirname, 'fixtures', 'fields-tags');

describe('ctags.createReadStream(tagsFileWithFields)', function() {
  it('returns a stream that emits data and end events', function() {
    var endHandler, stream, tags;
    stream = ctags.createReadStream(tagsFile);
    tags = [];
    stream.on('data', function(chunk) {
      return tags = tags.concat(chunk);
    });
    endHandler = jasmine.createSpy('endHandler');
    stream.on('end', endHandler);
    waitsFor(function() {
      return endHandler.callCount === 1;
    });
    return runs(function() {
      expect(tags.length).toBe(4);
      expect(tags[0].file).toBe('tagged.js');
      expect(tags[0].name).toBe('callMeMaybe');
      expect(tags[0].pattern).toBe('/^function callMeMaybe() {$/');
      expect(tags[0].kind).toBe('f');
      expect(tags[0].fields).toEqual({
        "class": 'TestClass',
        extra: 'test'
      });
      expect(tags[1].file).toBe('tagged-duplicate.js');
      expect(tags[1].name).toBe('duplicate');
      expect(tags[1].pattern).toBe('/^function duplicate() {$/');
      expect(tags[1].kind).toBe('f');
      expect(tags[1].fields).toEqual({
        test: 'spaces are fine',
        test2: 'more spaces'
      });
      expect(tags[2].file).toBe('tagged.js');
      expect(tags[2].name).toBe('duplicate');
      expect(tags[2].pattern).toBe('/^function duplicate() {$/');
      expect(tags[2].kind).toBe('f');
      expect(tags[2].fields).toEqual({
        'field name': '1'
      });
      expect(tags[3].file).toBe('tagged.js');
      expect(tags[3].name).toBe('thisIsCrazy');
      expect(tags[3].pattern).toBe('/^var thisIsCrazy = true;$/');
      expect(tags[3].kind).toBe('v');
      return expect(tags[3].fields).toEqual({
        emptyField: ''
      });
    });
  });
});
