package main

import (
	"context"
	"errors"
)

// AIInferenceRequest is the stable request passed from the native NPC binding
// to a future Go inference provider. It is intentionally independent from any
// particular model runtime.
type AIInferenceRequest struct {
	Protocol   int    `json:"protocol"`
	RequestID  uint64 `json:"request_id"`
	NPCID      string `json:"npc_id"`
	NPCName    string `json:"npc_name"`
	Persona    string `json:"persona"`
	PlayerText string `json:"player_text"`
	World      string `json:"world"`
}

type AIInferenceResponse struct {
	RequestID uint64 `json:"request_id"`
	Text      string `json:"text"`
	Provider  string `json:"provider"`
}

// AIProvider is the extension point for local models, HTTP inference servers,
// or another Go package. No provider is activated by the current server.
type AIProvider interface {
	Infer(context.Context, AIInferenceRequest) (AIInferenceResponse, error)
}

type DisabledAIProvider struct{}

func (DisabledAIProvider) Infer(context.Context, AIInferenceRequest) (AIInferenceResponse, error) {
	return AIInferenceResponse{}, errors.New("la inferencia de NPC todavía no está habilitada")
}
