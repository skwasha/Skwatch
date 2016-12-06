module.exports = [
  {
    "type": "heading",
    "defaultValue": "Skwatch Config"
  },
  {
    "type": "text",
    "defaultValue": "Set the Skwatch options here."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "color",
        "messageKey": "BackgroundColor",
        "defaultValue": "0x000000",
        "label": "Background Color",
        "sunlight": true,
        "layout": [
          [false, "0x000000", false],
          ["0x0055ff", "0x000000", "0x0000aa"],
          [false, "0x000000", false]
        ]
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
